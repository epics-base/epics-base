
/* * $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else 
#   define debugPrintf(argsInParen)
#endif

//
// the recv watchdog timer is active when this object is created
//
tcpRecvWatchdog::tcpRecvWatchdog 
    ( tcpiiu &iiuIn, double periodIn, osiTimerQueue & queueIn ) :
        osiTimer ( queueIn ),
    period ( periodIn ), iiu ( iiuIn ), responsePending ( false ),
        beaconAnomaly ( true )
{
}

tcpRecvWatchdog::~tcpRecvWatchdog ()
{
}

void tcpRecvWatchdog::expire ()
{
    if ( this->responsePending ) {
        this->cancel ();
        char hostName[128];
        this->iiu.hostName ( hostName, sizeof (hostName) );
        ca_printf ( "CA server \"%s\" unresponsive after %g inactive sec - disconnecting.\n", 
            hostName, this->period );
        this->iiu.forcedShutdown ();
    }
    else {
        this->responsePending = this->iiu.setEchoRequestPending ();
        debugPrintf ( ("TCP connection timed out - sending echo request\n") );
    }
}

void tcpRecvWatchdog::destroy ()
{
    // ignore timer destroy requests
}

bool tcpRecvWatchdog::again () const
{
    return true;
}

double tcpRecvWatchdog::delay () const
{
    if ( this->responsePending ) {
        return CA_ECHO_TIMEOUT;
    }
    else {
        return this->period;
    }
}

void tcpRecvWatchdog::beaconArrivalNotify ()
{
    if ( ! this->beaconAnomaly && ! this->responsePending ) {
        this->reschedule ( this->period );
        debugPrintf ( ("Saw a normal beacon - reseting TCP recv watchdog\n") );
    }
}

//
// be careful about using beacons to reset the connection
// time out watchdog until we have received a ping response 
// from the IOC (this makes the software detect reconnects
// faster when the server is rebooted twice in rapid 
// succession before a 1st or 2nd beacon has been received)
//
void tcpRecvWatchdog::beaconAnomalyNotify ()
{
    this->beaconAnomaly = true;
    debugPrintf ( ("Saw an abnormal beacon\n") );
}

void tcpRecvWatchdog::messageArrivalNotify ()
{
    this->beaconAnomaly = false;
    this->responsePending = false;
    this->reschedule ( this->period );
    debugPrintf ( ("received a message - reseting TCP recv watchdog\n") );
}

void tcpRecvWatchdog::connectNotify ()
{
    this->reschedule ( this->period );
    debugPrintf ( ("connected to the server - reseting TCP recv watchdog\n") );
}

const char *tcpRecvWatchdog::name () const
{
    return "TCP Receive Watchdog";
}

void tcpRecvWatchdog::cancel ()
{
    this->osiTimer::cancel ();
    debugPrintf ( ("canceling TCP recv watchdog\n") );
}

void tcpRecvWatchdog::show ( unsigned level ) const
{
    printf ( "Receive virtual circuit watchdog at %p, period %f\n",
        this, this->period );
    if ( level > 0u ) {
        printf ( "\tresponse pending boolean %u, beacon anomaly boolean %u\n",
            this->responsePending, this->beaconAnomaly );
    }
}
