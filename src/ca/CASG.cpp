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
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 */

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"
#include "autoPtrDestroy.h"
#include "cac.h"
#include "sgAutoPtr.h"

CASG::CASG ( ca_client_context &cacIn ) :
    client ( cacIn ), magic ( CASG_MAGIC )
{
    client.installCASG ( *this );
}

CASG::~CASG ()
{
    if ( this->verify () ) {
        this->reset ();
        this->client.uninstallCASG ( *this );
        this->magic = 0;
    }
    else {
        this->printf ("cac: attempt to destroy invalid sync group ignored\n");
    }
}

bool CASG::verify () const
{
    return ( this->magic == CASG_MAGIC );
}

/*
 * CASG::block ()
 */
int CASG::block ( double timeout )
{
    epicsTime cur_time;
    epicsTime beg_time;
    double delay;
    double remaining;
    int status;

    // prevent recursion nightmares by disabling blocking
    // for IO from within a CA callback. 
    if ( epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        return ECA_EVDISALLOW;
    }

    if ( timeout < 0.0 ) {
        return ECA_TIMEOUT;
    }

    cur_time = epicsTime::getCurrent ();

    this->client.flushRequest ();

    beg_time = cur_time;
    delay = 0.0;

    while ( 1 ) {
        if ( this->ioPendingList.count() == 0u ) {
            status = ECA_NORMAL;
            break;
        }

        remaining = timeout - delay;
        if ( remaining <= CAC_SIGNIFICANT_DELAY ) {
            /*
             * Make sure that we take care of
             * recv backlog at least once
             */
            status = ECA_TIMEOUT;
            break;
        }

        this->client.blockForEventAndEnableCallbacks ( this->sem, remaining );

        /*
         * force a time update 
         */
        cur_time = epicsTime::getCurrent ();

        delay = cur_time - beg_time;
    }

    this->reset ();

    return status;
}

void CASG::reset ()
{
    epicsGuard < casgMutex > locker ( this->mutex );
    this->destroyCompletedIO ();
    this->destroyPendingIO ();
}

// lock must be applied
void CASG::destroyCompletedIO ()
{
    tsDLList < syncGroupNotify > userStillRequestingList;
    syncGroupNotify *pNotify;
    while ( ( pNotify = this->ioCompletedList.get () ) ) {
        if ( pNotify->ioInitiated() ) {
            pNotify->destroy ( * this );
        }
        else {
            userStillRequestingList.add ( *pNotify );
        }
    }
    this->ioCompletedList.add ( userStillRequestingList );
}

// lock must be applied
void CASG::destroyPendingIO ()
{
    tsDLList < syncGroupNotify > userStillRequestingList;
    syncGroupNotify *pNotify;
    while ( ( pNotify = this->ioPendingList.get () ) ) {
        if ( pNotify->ioInitiated() ) {
            pNotify->destroy ( * this );
        }
        else {
            userStillRequestingList.add ( *pNotify );
        }
    }
    this->ioPendingList.add ( userStillRequestingList );
}

void CASG::show ( unsigned level ) const
{
    ::printf ( "Sync Group: id=%u, magic=%u, opPend=%u\n",
        this->getId (), this->magic, this->ioPendingList.count () );
    if ( level ) {
        epicsGuard < casgMutex > locker ( this->mutex );
        ::printf ( "\tPending" );
        tsDLIterConst < syncGroupNotify > notifyPending = this->ioPendingList.firstIter ();
        while ( notifyPending.valid () ) {
            notifyPending->show ( level - 1u );
            notifyPending++;
        }
        ::printf ( "\tCompleted" );
        tsDLIterConst < syncGroupNotify > notifyCompleted = this->ioCompletedList.firstIter ();
        while ( notifyCompleted.valid () ) {
            notifyCompleted->show ( level - 1u );
            notifyCompleted++;
        }
    }
}

bool CASG::ioComplete ()
{
    bool isCompleted;
    {
        epicsGuard < casgMutex > locker ( this->mutex );
        this->destroyCompletedIO ();
        isCompleted = ( this->ioPendingList.count () == 0u );
    }
    return isCompleted;
}

void CASG::put ( chid pChan, unsigned type, arrayElementCount count, const void * pValue )
{
    sgAutoPtr < syncGroupWriteNotify > pNotify ( *this );
    {
        epicsGuard < casgMutex > locker ( this->mutex );
        pNotify = syncGroupWriteNotify::factory ( 
            this->freeListWriteOP, *this, pChan );
        if ( pNotify.get () ) {
            this->ioPendingList.add ( *pNotify );
        }
        else {
            return;
        }
    }
    pNotify->begin ( type, count, pValue );
    pNotify.release ();
}

void CASG::get ( chid pChan, unsigned type, arrayElementCount count, void *pValue )
{
    sgAutoPtr < syncGroupReadNotify > pNotify ( *this );
    {
        epicsGuard < casgMutex > locker ( this->mutex );
        pNotify = syncGroupReadNotify::factory ( 
            this->freeListReadOP, *this, pChan, pValue );
        if ( pNotify.get () ) {
            this->ioPendingList.add ( *pNotify );
        }
        else {
            return;
        }
    }
    pNotify->begin ( type, count );
    pNotify.release ();
}

void CASG::destroyPendingIO ( syncGroupNotify * pNotify )
{
    if ( pNotify ) {
        epicsGuard < casgMutex > locker ( this->mutex );
        this->ioPendingList.remove ( *pNotify );
        pNotify->destroy ( *this );
    }
}

void CASG::completionNotify ( syncGroupNotify & notify )
{
    unsigned requestsIncomplete;
    {
        epicsGuard < casgMutex > locker ( this->mutex );
        this->ioPendingList.remove ( notify );
        this->ioCompletedList.add ( notify );
        requestsIncomplete = this->ioPendingList.count ();
    }
    if ( requestsIncomplete == 0u ) {
        this->sem.signal ();
    }
}

void CASG::recycleSyncGroupWriteNotify ( syncGroupWriteNotify & io )
{
    this->freeListWriteOP.release ( & io );
}

void CASG::recycleSyncGroupReadNotify ( syncGroupReadNotify & io )
{
    this->freeListReadOP.release ( & io );
}

int CASG::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->client.vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void CASG::exception ( int status, const char *pContext, 
    const char *pFileName, unsigned lineNo )
{
    this->client.exception ( status, pContext, pFileName, lineNo );
}

void CASG::exception ( int status, const char *pContext,
    const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
    unsigned type, arrayElementCount count, unsigned op )
{
    this->client.exception ( status, pContext, pFileName, 
        lineNo, chan, type, count, op );
}

void CASG::operator delete ( void * pCadaver )
{
    throw std::logic_error 
        ( "compiler is confused about placement delete" );
}


 
