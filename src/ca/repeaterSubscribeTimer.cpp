
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "repeaterSubscribeTimer.h"

#define epicsExportSharedSymbols
#include "udpiiu.h"
#undef epicsExportSharedSymbols

repeaterSubscribeTimer::repeaterSubscribeTimer ( udpiiu &iiuIn, epicsTimerQueue &queueIn ) :
    timer ( queueIn.createTimer () ), iiu ( iiuIn ), 
        attempts ( 0 ), registered ( false ), once ( false )
{
    this->timer.start ( *this, 10.0 );
}

repeaterSubscribeTimer::~repeaterSubscribeTimer ()
{
    this->timer.destroy ();
}

epicsTimerNotify::expireStatus repeaterSubscribeTimer::
        expire ( const epicsTime & /* currentTime */ )
{
    static const unsigned nTriesToMsg = 50;
    if ( this->attempts > nTriesToMsg && ! this->once ) {
        this->iiu.printf (
    "Unable to contact CA repeater after %u tries\n", nTriesToMsg);
        this->iiu.printf (
    "Silence this message by starting a CA repeater daemon\n");
        this->once = true;
    }

    this->iiu.repeaterRegistrationMessage ( this->attempts );
    this->attempts++;

    if ( this->registered ) {
        return noRestart;
    }
    else {
        return expireStatus ( restart, 1.0 );
    }
}

void repeaterSubscribeTimer::show ( unsigned /* level */ ) const
{
}

void repeaterSubscribeTimer::confirmNotify ()
{
    this->registered = true;
}
