
/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 *
 */

#include "iocinf.h"

repeaterSubscribeTimer::repeaterSubscribeTimer ( udpiiu &iiuIn, osiTimerQueue &queueIn ) :
    osiTimer ( 10.0, queueIn ), iiu ( iiuIn ), attempts ( 0 ), registered ( false ), once (false)
{
}

void repeaterSubscribeTimer::expire ()
{
    static const unsigned nTriesToMsg = 50;
    this->attempts++;
    if ( this->attempts > nTriesToMsg && ! this->once ) {
        ca_printf (
    "Unable to contact CA repeater after %u tries\n", nTriesToMsg);
        ca_printf (
    "Silence this message by starting a CA repeater daemon\n");
        this->once = true;
    }

    this->iiu.repeaterRegistrationMessage ( this->attempts );
}

void repeaterSubscribeTimer::destroy ()
{
}

bool repeaterSubscribeTimer::again () const
{
    return ( ! this->registered );
}

double repeaterSubscribeTimer::delay () const
{
    return 1.0;
}

void repeaterSubscribeTimer::show (unsigned /* level */ ) const
{
}

const char *repeaterSubscribeTimer::name () const
{
    return "repeaterSubscribeTimer";
}

void repeaterSubscribeTimer::confirmNotify ()
{
    this->registered = true;
}
