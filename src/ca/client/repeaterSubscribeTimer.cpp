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
 *
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "cac.h"
#include "iocinf.h"
#include "repeaterSubscribeTimer.h"

#define epicsExportSharedSymbols
#include "udpiiu.h"
#undef epicsExportSharedSymbols

static const double repeaterSubscribeTimerInitialPeriod = 10.0; // sec
static const double repeaterSubscribeTimerPeriod = 1.0; // sec

repeaterSubscribeTimer::repeaterSubscribeTimer ( 
    repeaterTimerNotify & iiuIn, epicsTimerQueue & queueIn,
    epicsMutex & cbMutexIn, cacContextNotify & ctxNotifyIn ) :
    timer ( queueIn.createTimer () ), iiu ( iiuIn ), 
        cbMutex ( cbMutexIn ),ctxNotify ( ctxNotifyIn ),
        attempts ( 0 ), registered ( false ), once ( false )
{
}

repeaterSubscribeTimer::~repeaterSubscribeTimer ()
{
    this->timer.destroy ();
}

void repeaterSubscribeTimer::start ()
{
    this->timer.start ( 
        *this, repeaterSubscribeTimerInitialPeriod );
}

void repeaterSubscribeTimer::shutdown (
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    epicsGuardRelease < epicsMutex > unguard ( guard );
    {
        epicsGuardRelease < epicsMutex > cbUnguard ( cbGuard );
        this->timer.cancel ();
    }
}

epicsTimerNotify::expireStatus repeaterSubscribeTimer::
    expire ( const epicsTime & /* currentTime */ )
{
    epicsGuard < epicsMutex > guard ( this->stateMutex );
    
    static const unsigned nTriesToMsg = 50;
    if ( this->attempts > nTriesToMsg && ! this->once ) {
        callbackManager mgr ( this->ctxNotify, this->cbMutex );
        this->iiu.printFormated ( mgr.cbGuard,
    "CA client library is unable to contact CA repeater after %u tries.\n", 
            nTriesToMsg );
        this->iiu.printFormated ( mgr.cbGuard,
    "Silence this message by starting a CA repeater daemon\n") ;
        this->iiu.printFormated ( mgr.cbGuard,
    "or by calling ca_pend_event() and or ca_poll() more often.\n" );
        this->once = true;
    }

    this->iiu.repeaterRegistrationMessage ( this->attempts );
    this->attempts++;

    if ( this->registered ) {
        return noRestart;
    }
    else {
        return expireStatus ( restart, repeaterSubscribeTimerPeriod );
    }
}

void repeaterSubscribeTimer::show ( unsigned /* level */ ) const
{
    epicsGuard < epicsMutex > guard ( this->stateMutex );
    
    ::printf ( "repeater subscribe timer: attempts=%u registered=%u once=%u\n",
        this->attempts, this->registered, this->once );
}

void repeaterSubscribeTimer::confirmNotify ()
{
    epicsGuard < epicsMutex > guard ( this->stateMutex );
    this->registered = true;
}

repeaterTimerNotify::~repeaterTimerNotify () {}
