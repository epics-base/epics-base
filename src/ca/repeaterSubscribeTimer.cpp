
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

repeaterSubscribeTimer::repeaterSubscribeTimer (udpiiu &iiuIn, osiTimerQueue &queueIn) :
    osiTimer (queueIn), iiu (iiuIn)
{
}

void repeaterSubscribeTimer::expire ()
{
    this->iiu.contactRepeater = 1u;
    semBinaryGive (this->iiu.xmitSignal);
}

void repeaterSubscribeTimer::destroy ()
{
}

bool repeaterSubscribeTimer::again () const
{
    if (this->iiu.repeaterContacted) {
        return false;
    }
    else {
        return true;
    }
}

double repeaterSubscribeTimer::delay () const
{
    return REPEATER_TRY_PERIOD;
}

void repeaterSubscribeTimer::show (unsigned /* level */ ) const
{
}

const char *repeaterSubscribeTimer::name () const
{
    return "repeaterSubscribeTimer";
}

