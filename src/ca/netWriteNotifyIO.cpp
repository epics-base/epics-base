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

netWriteNotifyIO::netWriteNotifyIO ( 
    privateInterfaceForIO & ioComplIntf, cacWriteNotify & notifyIn ) :
    notify ( notifyIn ), privateChanForIO ( ioComplIntf )
{
}

netWriteNotifyIO::~netWriteNotifyIO ()
{
}

void netWriteNotifyIO::show ( unsigned /* level */ ) const
{
    ::printf ( "read write notify IO at %p\n", 
        static_cast < const void * > ( this ) );
}

void netWriteNotifyIO::show ( 
    epicsGuard < epicsMutex > &,  
    unsigned level ) const
{
    this->show ( level );
}

void netWriteNotifyIO::destroy ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle )
{
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( guard, *this );
}

void netWriteNotifyIO::completion (
    epicsGuard < epicsMutex > & guard,
    cacRecycle & recycle )
{
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.completion ( guard );
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( guard, *this );
}

void netWriteNotifyIO::completion ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle,
    unsigned /* type */, arrayElementCount /* count */, 
    const void * /* pData */ )
{
    //this->chan.getClient().printf ( "Write response with data ?\n" );
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( guard, *this );
}

void netWriteNotifyIO::exception ( 
    epicsGuard < epicsMutex > & guard,
    cacRecycle & recycle,
    int status, const char * pContext )
{
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.exception ( 
        guard, status, pContext, UINT_MAX, 0u );
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( guard, *this );
}

void netWriteNotifyIO::exception ( 
    epicsGuard < epicsMutex > & guard, 
    cacRecycle & recycle,
    int status, const char *pContext, 
    unsigned type, arrayElementCount count )
{
    this->privateChanForIO.ioCompletionNotify ( guard, *this );
    this->notify.exception ( 
        guard, status, pContext, type, count );
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( guard, *this );
}

class netSubscription * netWriteNotifyIO::isSubscription ()
{
    return 0;
}

void netWriteNotifyIO::forceSubscriptionUpdate (
    epicsGuard < epicsMutex > &, nciu & )
{
}

void netWriteNotifyIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

