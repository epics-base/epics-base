
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
    (double periodIn, osiTimerQueue & queueIn, bool echoProtocolAcceptedIn) :
    osiTimer (periodIn, queueIn),
    period (periodIn),
    echoProtocolAccepted (echoProtocolAcceptedIn),
    echoResponsePending (false)
{
}

void tcpRecvWatchdog::echoResponseNotify ()
{
    this->echoResponsePending = false;
    this->reschedule ( this->period );
}

void tcpRecvWatchdog::expire ()
{
    /*
     * remain backwards compatible with old servers
     * ( this isnt an echo request )
     */
    if ( ! this->echoProtocolAccepted ) {
        this->noopRequestMsg ();
    }
    else if ( this->echoResponsePending ) {
        ca_printf ( "CA server unresponsive for %f sec. Disconnecting\n", this->period );
        this->shutdown ();
    }
    else {
        this->echoRequestMsg ();
        this->echoResponsePending = true;
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
    if (this->echoResponsePending) {
        return CA_ECHO_TIMEOUT;
    }
    else {
        return this->period;
    }
}

const char *tcpRecvWatchdog::name () const
{
    return "TCP Receive Watchdog";
}
