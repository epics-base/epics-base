
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
    (double periodIn, osiTimerQueue & queueIn) :
    osiTimer (queueIn),
    period (periodIn)
{
}

void tcpSendWatchdog::expire ()
{
    ca_printf ("Unable to deliver message for %f sec. Disconnecting from CA server\n", this->period);
    this->shutdown ();
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

void tcpSendWatchdog::rescheduleSendTimer ()
{
    this->reschedule ();
}