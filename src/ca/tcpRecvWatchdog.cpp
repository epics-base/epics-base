
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

tcpRecvWatchdog::tcpRecvWatchdog 
    ( double periodIn, osiTimerQueue & queueIn, bool echoProtocolAcceptedIn ) :
            osiTimer ( queueIn ),
    period ( periodIn ),
    echoProtocolAccepted ( echoProtocolAcceptedIn ),
    responsePending ( false ),
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
        this->hostName ( hostName, sizeof (hostName) );
        ca_printf ( "CA server %s unresponsive for %g sec. Disconnecting.\n", 
            hostName, this->period + CA_ECHO_TIMEOUT );
        this->forcedShutdown ();
    }
    else {
        this->echoRequest ();
        if ( this->echoProtocolAccepted ) {
            this->responsePending = true;
        }
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
    }
}

/*
 * be careful about using beacons to reset the connection
 * time out watchdog until we have received a ping response 
 * from the IOC (this makes the software detect reconnects
 * faster when the server is rebooted twice in rapid 
 * succession before a 1st or 2nd beacon has been received)
 */
void tcpRecvWatchdog::beaconAnomalyNotify ()
{
    this->beaconAnomaly = true;
}

void tcpRecvWatchdog::messageArrivalNotify ()
{
    this->beaconAnomaly = false;
    this->responsePending = false;
    this->reschedule ( this->period );
}

void tcpRecvWatchdog::connectNotify ()
{
    this->reschedule ( this->period );
}

const char *tcpRecvWatchdog::name () const
{
    return "TCP Receive Watchdog";
}

void tcpRecvWatchdog::cancelRecvWatchdog ()
{
    this->cancel ();
}

void tcpRecvWatchdog::show ( unsigned level ) const
{
    printf ( "Receive virtual circuit watchdog at %p, period %f\n",
        this, this->period );
    if ( level > 0u ) {
        printf ( "\tresponse pending boolean %u, beacon anomaly boolean %u\n",
            this->responsePending, this->beaconAnomaly );
    }
    if ( level > 1u ) {
        printf ( "\techo protocol accepted boolean %u\n",
            this->echoProtocolAccepted );
    }
}
