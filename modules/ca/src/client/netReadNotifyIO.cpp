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

#include "iocinf.h"
#include "nciu.h"
#include "cac.h"

netReadNotifyIO::netReadNotifyIO ( 
    privateInterfaceForIO & ioComplIntfIn, 
        cacReadNotify & notify ) :
    notify ( notify ), privateChanForIO ( ioComplIntfIn )
{
}

netReadNotifyIO::~netReadNotifyIO ()
{
}

void netReadNotifyIO::show ( unsigned /* level */ ) const
{
    ::printf ( "netReadNotifyIO at %p\n", 
        static_cast < const void * > ( this ) );
}

void netReadNotifyIO::show ( 
    epicsGuard < epicsMutex > &, unsigned level ) const
{
    this->show ( level );
}

void netReadNotifyIO::destroy ( 
    epicsGuard < epicsMutex > & guard, cacRecycle & recycle )
{
    this->~netReadNotifyIO ();
    recycle.recycleReadNotifyIO ( guard, *this );
}

void netReadNotifyIO::completion ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle, unsigned type, 
    arrayElementCount count, const void * pData )
{
    //guard.assertIdenticalMutex ( this->mutex );
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.completion ( guard, type, count, pData );
    this->~netReadNotifyIO ();
    recycle.recycleReadNotifyIO ( guard, *this );
}

void netReadNotifyIO::completion (
    epicsGuard < epicsMutex > & guard,
    cacRecycle & recycle )
{
    //guard.assertIdenticalMutex ( this->mutex );
    //this->chan.getClient().printf ( "Read response w/o data ?\n" );
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->~netReadNotifyIO ();
    recycle.recycleReadNotifyIO ( guard, *this );
}

void netReadNotifyIO::exception ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle,
    int status, const char *pContext )
{
    //guard.assertIdenticalMutex ( this->mutex );
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.exception ( 
        guard, status, pContext, UINT_MAX, 0u );
    this->~netReadNotifyIO ();
    recycle.recycleReadNotifyIO ( guard, *this );
}

void netReadNotifyIO::exception ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle,
    int status, const char *pContext, 
    unsigned type, arrayElementCount count )
{
    //guard.assertIdenticalMutex ( this->mutex )
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.exception ( 
        guard, status, pContext, type, count );
    this->~netReadNotifyIO ();
    recycle.recycleReadNotifyIO ( guard, *this );
}

class netSubscription * netReadNotifyIO::isSubscription ()
{
    return 0;
}

void netReadNotifyIO::forceSubscriptionUpdate (
    epicsGuard < epicsMutex > &, nciu & )
{
}

void netReadNotifyIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}



