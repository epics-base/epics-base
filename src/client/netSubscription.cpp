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
 */

#include <string>
#include <stdexcept>

#include "errlog.h"

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "nciu.h"
#include "cac.h"
#include "db_access.h" // for dbf_type_to_text
#include "caerr.h"

netSubscription::netSubscription ( 
        privateInterfaceForIO & chanIn, 
        unsigned typeIn, arrayElementCount countIn, 
        unsigned maskIn, cacStateNotify & notifyIn ) :
    count ( countIn ), privateChanForIO ( chanIn ),
    notify ( notifyIn ), type ( typeIn ), mask ( maskIn ),
    subscribed ( false )
{
    if ( ! dbr_type_is_valid ( typeIn ) ) {
        throw cacChannel::badType ();
    }
    if ( this->mask == 0u ) {
        throw cacChannel::badEventSelection ();
    }
}

netSubscription::~netSubscription () 
{
}

void netSubscription::destroy ( 
    epicsGuard < epicsMutex > & guard, cacRecycle & recycle )
{
    this->~netSubscription ();
    recycle.recycleSubscription ( guard, *this );
}

class netSubscription * netSubscription::isSubscription ()
{
    return this;
}

void netSubscription::show ( unsigned /* level */ ) const
{
    ::printf ( "event subscription IO at %p, type %s, element count %lu, mask %u\n", 
        static_cast < const void * > ( this ), 
        dbf_type_to_text ( static_cast < int > ( this->type ) ), 
        this->count, this->mask );
}

void netSubscription::show ( 
    epicsGuard < epicsMutex > &, unsigned level ) const
{
    this->show ( level );
}

void netSubscription::completion ( 
    epicsGuard < epicsMutex > &, cacRecycle & )
{
    errlogPrintf ( "subscription update w/o data ?\n" );
}

void netSubscription::exception ( 
    epicsGuard < epicsMutex > & guard, cacRecycle & recycle, 
    int status, const char * pContext )
{
    if ( status == ECA_DISCONN ) {
        this->subscribed = false;
    }
    if ( status == ECA_CHANDESTROY ) {
        this->privateChanForIO.ioCompletionNotify ( guard, *this );
        this->notify.exception ( 
            guard, status, pContext, UINT_MAX, 0 );
        this->~netSubscription ();
        recycle.recycleSubscription ( guard, *this );
    }
    else {
        // guard.assertIdenticalMutex ( this->mutex );
        if ( this->privateChanForIO.connected ( guard )  ) {
            this->notify.exception ( 
                guard, status, pContext, UINT_MAX, 0 );
        }
    }
}

void netSubscription::exception ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle, int status, const char * pContext, 
    unsigned typeIn, arrayElementCount countIn )
{
    if ( status == ECA_DISCONN ) {
        this->subscribed = false;
    }
    if ( status == ECA_CHANDESTROY ) {
        this->privateChanForIO.ioCompletionNotify ( guard, *this );
        this->notify.exception ( 
            guard, status, pContext, UINT_MAX, 0 );
        this->~netSubscription ();
        recycle.recycleSubscription ( guard, *this );
    }
    else {
        //guard.assertIdenticalMutex ( this->mutex );
        if ( this->privateChanForIO.connected ( guard ) ) {
            this->notify.exception ( 
                guard, status, pContext, typeIn, countIn );
        }
    }
}

void netSubscription::completion ( 
    epicsGuard < epicsMutex > & guard, cacRecycle &,
    unsigned typeIn, arrayElementCount countIn, 
    const void * pDataIn )
{
    // guard.assertIdenticalMutex ( this->mutex );
    if ( this->privateChanForIO.connected ( guard )  ) {
        this->notify.current ( 
            guard, typeIn, countIn, pDataIn );
    }
}

void netSubscription::subscribeIfRequired (
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    if ( ! this->subscribed ) {
        chan.getPIIU(guard)->subscriptionRequest ( 
            guard, chan, *this );
        this->subscribed = true;
    }
}

void netSubscription::unsubscribeIfRequired ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    if ( this->subscribed ) {
        chan.getPIIU(guard)->subscriptionCancelRequest ( 
            guard, chan, *this );
        this->subscribed = false;
    }
}

void netSubscription::forceSubscriptionUpdate (
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    chan.getPIIU(guard)->subscriptionUpdateRequest ( 
        guard, chan, *this );
}

void netSubscription::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}







