
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

tcpSendWatchdog::tcpSendWatchdog 
    ( tcpiiu &iiuIn, double periodIn, osiTimerQueue & queueIn ) :
    osiTimer ( queueIn ), period ( periodIn ), iiu ( iiuIn )
{
}

tcpSendWatchdog::~tcpSendWatchdog ()
{
}

void tcpSendWatchdog::expire ()
{
    char hostName[128];
    this->iiu.hostName ( hostName, sizeof (hostName) );
    ca_printf ( "Request not accepted by CA server %s for %g sec. Disconnecting.\n", 
        hostName, this->period);
    this->iiu.forcedShutdown ();
}

void tcpSendWatchdog::destroy ()
{
    // ignore timer destroy requests
}

bool tcpSendWatchdog::again () const
{
    return false; // a one shot
}

double tcpSendWatchdog::delay () const
{
    return this->period;
}

const char *tcpSendWatchdog::name () const
{
    return "TCP Send Watchdog";
}

void tcpSendWatchdog::arm ()
{
    this->osiTimer::reschedule ();
}

void tcpSendWatchdog::cancel ()
{
    this->osiTimer::cancel ();
}