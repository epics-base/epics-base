/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "cac.h"
#include "virtualCircuit.h"

//
// the recv watchdog timer is active when this object is created
//
tcpRecvWatchdog::tcpRecvWatchdog 
    ( epicsMutex & cbMutexIn, cacContextNotify & ctxNotifyIn,
            epicsMutex & mutexIn, tcpiiu & iiuIn, 
            double periodIn, epicsTimerQueue & queueIn ) :
        period ( periodIn ), timer ( queueIn.createTimer () ),
        cbMutex ( cbMutexIn ), ctxNotify ( ctxNotifyIn ), 
        mutex ( mutexIn ), iiu ( iiuIn ), 
        probeResponsePending ( false ), beaconAnomaly ( true ), 
        probeTimeoutDetected ( false ), shuttingDown ( false )
{
}

tcpRecvWatchdog::~tcpRecvWatchdog ()
{
    this->timer.destroy ();
}

epicsTimerNotify::expireStatus
tcpRecvWatchdog::expire ( const epicsTime & /* currentTime */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->shuttingDown ) {
        return noRestart;
    }
    if ( this->probeResponsePending ) {
        if ( this->iiu.receiveThreadIsBusy ( guard ) ) {
            return expireStatus ( restart, CA_ECHO_TIMEOUT );
        }

        {
#           ifdef DEBUG
                char hostName[128];
                this->iiu.getHostName ( guard, hostName, sizeof (hostName) );
                debugPrintf ( ( "CA server \"%s\" unresponsive after %g inactive sec"
                            "- disconnecting.\n", 
                    hostName, this->period ) );
#           endif
            // to get the callback lock safely we must reorder 
            // the lock hierarchy
            epicsGuardRelease < epicsMutex > unguard ( guard );
            {
                // callback lock is required because channel disconnect
                // state change is initiated from this thread, and 
                // this can cause their disconnect notify callback
                // to be invoked.
                callbackManager mgr ( this->ctxNotify, this->cbMutex );
                epicsGuard < epicsMutex > tmpGuard ( this->mutex );
                this->iiu.receiveTimeoutNotify ( mgr, tmpGuard );
                this->probeTimeoutDetected = true;
            }
        }
        return noRestart;
    }
    else {
        if ( this->iiu.receiveThreadIsBusy ( guard ) ) {
            return expireStatus ( restart, this->period );
        }
        this->probeTimeoutDetected = false;
        this->probeResponsePending = this->iiu.setEchoRequestPending ( guard );
        debugPrintf ( ("circuit timed out - sending echo request\n") );
        return expireStatus ( restart, CA_ECHO_TIMEOUT );
    }
}

void tcpRecvWatchdog::beaconArrivalNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( ! ( this->shuttingDown || this->beaconAnomaly || this->probeResponsePending ) ) {
        this->timer.start ( *this, this->period );
        debugPrintf ( ("saw a normal beacon - reseting circuit receive watchdog\n") );
    }
}

//
// be careful about using beacons to reset the connection
// time out watchdog until we have received a ping response 
// from the IOC (this makes the software detect reconnects
// faster when the server is rebooted twice in rapid 
// succession before a 1st or 2nd beacon has been received)
//
void tcpRecvWatchdog::beaconAnomalyNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->beaconAnomaly = true;
    debugPrintf ( ("Saw an abnormal beacon\n") );
}

void tcpRecvWatchdog::messageArrivalNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! ( this->shuttingDown || this->probeResponsePending ) ) {
        this->beaconAnomaly = false;
        this->timer.start ( *this, this->period );
        debugPrintf ( ("received a message - reseting circuit recv watchdog\n") );
    }
}

void tcpRecvWatchdog::probeResponseNotify ( 
    epicsGuard < epicsMutex > & cbGuard )
{
    bool restartNeeded = false;
    double restartDelay = DBL_MAX;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        if ( this->probeResponsePending && ! this->shuttingDown ) {
            restartNeeded = true;
            if ( this->probeTimeoutDetected ) {
                this->probeTimeoutDetected = false;
                this->probeResponsePending = this->iiu.setEchoRequestPending ( guard );
                restartDelay = CA_ECHO_TIMEOUT;
                debugPrintf ( ("late probe response - sending another probe request\n") );
            }
            else {
                this->probeResponsePending = false;
                restartDelay = this->period;
                this->iiu.responsiveCircuitNotify ( cbGuard, guard );
                debugPrintf ( ("probe response on time - circuit was tagged reponsive if unresponsive\n") );
            }
        }
    }
    if ( restartNeeded ) {
        this->timer.start ( *this, restartDelay );
        debugPrintf ( ("recv wd restarted with delay %f\n", restartDelay) );
    }
}

//
// The thread for outgoing requests in the client runs 
// at a higher priority than the thread in the client
// that receives responses. Therefore, there could 
// be considerable large array write send backlog that 
// is delaying departure of an echo request and also 
// interrupting delivery of an echo response. 
// We must be careful not to timeout the echo response as 
// long as we see indication of regular departures of  
// message buffers from the client in a situation where 
// we know that the TCP send queueing has been exceeded. 
// The send watchdog will be responsible for detecting 
// dead connections in this case.
//
void tcpRecvWatchdog::sendBacklogProgressNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    // We dont set "beaconAnomaly" to be false here because, after we see a
    // beacon anomaly (which could be transiently detecting a reboot) we will 
    // not trust the beacon as an indicator of a healthy server until we 
    // receive at least one message from the server.
    if ( this->probeResponsePending && ! this->shuttingDown ) {
        this->timer.start ( *this, CA_ECHO_TIMEOUT );
        debugPrintf ( ("saw heavy send backlog - reseting circuit recv watchdog\n") );
    }
}

void tcpRecvWatchdog::connectNotify (
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->shuttingDown ) {
        return;
    }
    this->timer.start ( *this, this->period );
    debugPrintf ( ("connected to the server - initiating circuit recv watchdog\n") );
}

void tcpRecvWatchdog::sendTimeoutNotify ( 
    epicsGuard < epicsMutex > & /* cbGuard */,
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    bool restartNeeded = false;
    if ( ! ( this->probeResponsePending || this->shuttingDown ) ) {
        this->probeResponsePending = this->iiu.setEchoRequestPending ( guard );
        restartNeeded = true;
    }
    if ( restartNeeded ) {
        this->timer.start ( *this, CA_ECHO_TIMEOUT );
    }
    debugPrintf ( ("TCP send timed out - sending echo request\n") );
}

void tcpRecvWatchdog::cancel ()
{
    this->timer.cancel ();
    debugPrintf ( ("canceling TCP recv watchdog\n") );
}

void tcpRecvWatchdog::shutdown ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->shuttingDown = true;
    }
    this->timer.cancel ();
    debugPrintf ( ("canceling TCP recv watchdog\n") );
}

double tcpRecvWatchdog::delay () const
{
    return this->timer.getExpireDelay ();
}

void tcpRecvWatchdog::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    ::printf ( "Receive virtual circuit watchdog at %p, period %f\n",
        static_cast <const void *> ( this ), this->period );
    if ( level > 0u ) {
        ::printf ( "\t%s %s %s\n",
            this->probeResponsePending ? "probe-response-pending" : "", 
            this->beaconAnomaly ? "beacon-anomaly-detected" : "",
            this->probeTimeoutDetected ? "probe-response-timeout" : "" );
    }
}

