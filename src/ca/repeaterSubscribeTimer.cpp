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

#include "cac.h"
#include "iocinf.h"
#include "repeaterSubscribeTimer.h"

#define epicsExportSharedSymbols
#include "udpiiu.h"
#undef epicsExportSharedSymbols

repeaterSubscribeTimer::repeaterSubscribeTimer ( 
    udpiiu & iiuIn, epicsTimerQueue & queueIn,
    epicsMutex & cbMutexIn, cacContextNotify & ctxNotifyIn ) :
    timer ( queueIn.createTimer () ), iiu ( iiuIn ), 
        cbMutex ( cbMutexIn ),ctxNotify ( ctxNotifyIn ),
        attempts ( 0 ), registered ( false ), once ( false )
{
    this->timer.start ( *this, 10.0 );
}

repeaterSubscribeTimer::~repeaterSubscribeTimer ()
{
    this->timer.destroy ();
}

void repeaterSubscribeTimer::shutdown ()
{
    this->timer.cancel ();
}

epicsTimerNotify::expireStatus repeaterSubscribeTimer::
    expire ( const epicsTime & /* currentTime */ )  // X aCC 361
{
    static const unsigned nTriesToMsg = 50;
    if ( this->attempts > nTriesToMsg && ! this->once ) {
        callbackManager mgr ( this->ctxNotify, this->cbMutex );
        this->iiu.printf ( mgr.cbGuard,
    "CA client library is unable to contact CA repeater after %u tries.\n", 
            nTriesToMsg);
        this->iiu.printf ( mgr.cbGuard,
    "Silence this message by starting a CA repeater daemon\n");
        this->iiu.printf ( mgr.cbGuard,
    "or by calling ca_pend_event() and or ca_poll() more often.\n");
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
