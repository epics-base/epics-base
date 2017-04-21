/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*  
 *  Author: 
 *  Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#include <string>
#include <stdexcept>

#include <limits.h>
#include <string.h>

#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "errMdef.h"
#include "errlog.h"

#include "caerr.h" // this needs to be eliminated
#include "db_access.h" // this needs to be eliminated

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

dbPutNotifyBlocker::dbPutNotifyBlocker ( epicsMutex & mutexIn ) :
    mutex ( mutexIn ), pNotify ( 0 ),
    maxValueSize ( sizeof ( this->dbrScalarValue ) )
{
    memset ( & this->pn, '\0', sizeof ( this->pn ) );
    memset ( & this->dbrScalarValue, '\0', sizeof ( this->dbrScalarValue ) );
    this->pbuffer = & this->dbrScalarValue;
}

dbPutNotifyBlocker::~dbPutNotifyBlocker ()
{
}

void dbPutNotifyBlocker::destructor ( CallbackGuard & cbGuard, 
                                  epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->cancel ( cbGuard, guard );
    if ( this->maxValueSize > sizeof ( this->dbrScalarValue ) ) {
        char * pBuf = static_cast < char * > ( this->pbuffer );
        delete [] pBuf;
    }
    this->~dbPutNotifyBlocker ();
}

void dbPutNotifyBlocker::cancel (
    CallbackGuard & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->pNotify ) {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        dbNotifyCancel ( &this->pn );
    }
    this->pNotify = 0;
    this->block.signal ();
}

void dbPutNotifyBlocker::expandValueBuf (
    epicsGuard < epicsMutex > & guard, unsigned long newSize )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->maxValueSize < newSize ) {
        if ( this->maxValueSize > sizeof ( this->dbrScalarValue ) ) {
            char * pBuf = static_cast < char * > ( this->pbuffer );
            delete [] pBuf;
            this->maxValueSize = sizeof ( this->dbrScalarValue );
            this->pbuffer = & this->dbrScalarValue;
        }
        this->pbuffer = new char [newSize];
        this->maxValueSize = newSize;
    }
}

extern "C" int putNotifyPut ( processNotify *ppn, notifyPutType type )
{
    if(ppn->status==notifyCanceled) return 0;
/*
 * No locking in this method because only a dbNotifyCancel could interrupt
 * and it does not return until cancel is done.
 */
    dbPutNotifyBlocker * pBlocker = static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    return db_put_process(ppn,type,
        pBlocker->dbrType,pBlocker->pbuffer,pBlocker->nRequest);
}

extern "C" void putNotifyCompletion ( processNotify *ppn )
{
    dbPutNotifyBlocker * const pBlocker = 
            static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    epicsGuard < epicsMutex > guard ( pBlocker->mutex );
    cacWriteNotify * const pNtfy = pBlocker->pNotify;
    if ( pNtfy ) {
        pBlocker->pNotify = 0;
        // Its necessary to signal the initiators now before we call
        // the user callback. This is less efficent, and potentially
        // causes more thread context switching, but its probably 
        // unavoidable because its possible that the use callback 
        // might destroy this object.
        pBlocker->block.signal ();
        if ( pBlocker->pn.status != notifyOK ) {
            pNtfy->exception ( 
                guard, ECA_PUTFAIL,  "put notify unsuccessful",
                static_cast < unsigned > (pBlocker->dbrType), 
                static_cast < unsigned > (pBlocker->nRequest) );
        }
        else {
            pNtfy->completion ( guard );
        }
    }
    else {
        errlogPrintf ( "put notify completion with nill pNotify?\n" );
    }
}

void dbPutNotifyBlocker::initiatePutNotify (
    epicsGuard < epicsMutex > & guard, cacWriteNotify & notify,
    struct dbChannel * dbch, unsigned type, unsigned long count,
    const void * pValue )
{
    guard. assertIdenticalMutex ( this->mutex );
    epicsTime begin;
    bool beginTimeInit = false;
    while ( true ) {
        if ( this->pNotify == 0 ) {
            this->pNotify = & notify;
            break;
        }
        if ( beginTimeInit ) {
            if ( epicsTime::getCurrent () - begin > 30.0 ) {
                throw cacChannel::requestTimedOut ();
            }
        }
        else {
            begin = epicsTime::getCurrent ();
            beginTimeInit = true;
        }
        {
            epicsGuardRelease < epicsMutex > autoRelease ( guard );
            this->block.wait ( 1.0 );
        }
    }

    if ( count > LONG_MAX ) {
        throw cacChannel::outOfBounds();
    }

    if ( type > SHRT_MAX ) {
        throw cacChannel::badType();
    }

    this->dbrType = type;
    this->nRequest = static_cast < unsigned > ( count );
    this->pn.requestType = putProcessRequest;
    this->pn.chan = dbch;
    this->pn.putCallback = putNotifyPut;
    this->pn.doneCallback = putNotifyCompletion;
    this->pn.usrPvt = this;

    unsigned long size = dbr_size_n ( type, count );
    this->expandValueBuf ( guard, size );
    memcpy ( this->pbuffer, pValue, size );

    {
        epicsGuardRelease < epicsMutex > autoRelease ( guard );
        ::dbProcessNotify ( &this->pn );
    }
}

void dbPutNotifyBlocker::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->show ( guard, level );
}

void dbPutNotifyBlocker::show (
    epicsGuard < epicsMutex > &, unsigned level ) const
{
    printf ( "put notify blocker at %p\n",
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        this->block.show ( level - 1u );
    }
}

dbSubscriptionIO * dbPutNotifyBlocker::isSubscription ()
{
    return 0;
}

void * dbPutNotifyBlocker::operator new ( size_t size,
    tsFreeList < dbPutNotifyBlocker, 64, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void dbPutNotifyBlocker::operator delete ( void *pCadaver,
    tsFreeList < dbPutNotifyBlocker, 64, epicsMutexNOOP > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

void dbPutNotifyBlocker::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}
