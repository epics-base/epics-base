/*
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"
#include "autoPtrDestroy.h"
#include "cac.h"

template class tsFreeList < CASG, 128 >;
template class epicsSingleton < tsFreeList < struct CASG, 128 > >;
template class tsFreeList < syncGroupReadNotify, 128, epicsMutexNOOP >;
template class tsFreeList < syncGroupWriteNotify, 128, epicsMutexNOOP >;

epicsSingleton < tsFreeList < struct CASG, 128 > > CASG::pFreeList;

CASG::CASG ( oldCAC &cacIn ) :
    client ( cacIn ), magic ( CASG_MAGIC )
{
    client.installCASG ( *this );
}

void CASG::destroy ()
{
    delete this;
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

        status = this->client.blockForEventAndEnableCallbacks ( this->sem, remaining );
        if ( status != ECA_NORMAL ) {
            break;
        }

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
    epicsGuard < epicsMutex > locker ( this->mutex );
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
        epicsGuard < epicsMutex > locker ( this->mutex );
        ::printf ( "\tPending" );
        tsDLIterConstBD < syncGroupNotify > notifyPending = this->ioPendingList.firstIter ();
        while ( notifyPending.valid () ) {
            notifyPending->show ( level - 1u );
            notifyPending++;
        }
        ::printf ( "\tCompleted" );
        tsDLIterConstBD < syncGroupNotify > notifyCompleted = this->ioCompletedList.firstIter ();
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
        epicsGuard < epicsMutex > locker ( this->mutex );
        this->destroyCompletedIO ();
        isCompleted = ( this->ioPendingList.count () == 0u );
    }
    return isCompleted;
}

int CASG::put ( chid pChan, unsigned type, arrayElementCount count, const void *pValue )
{
    syncGroupWriteNotify * pNotify = 0;
    try {
        {
            epicsGuard < epicsMutex > locker ( this->mutex );
            pNotify = syncGroupWriteNotify::factory ( 
                this->freeListWriteOP, *this, pChan );
            if ( pNotify ) {
                this->ioPendingList.add ( *pNotify );
            }
            else {
                return ECA_ALLOCMEM;
            }
        }
        pNotify->begin ( type, count, pValue );
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        destroyPendingIO ( pNotify );
        return ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        destroyPendingIO ( pNotify );
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        destroyPendingIO ( pNotify );
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        destroyPendingIO ( pNotify );
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        destroyPendingIO ( pNotify );
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        destroyPendingIO ( pNotify );
        return ECA_INTERNAL;
    }
}

int CASG::get ( chid pChan, unsigned type, arrayElementCount count, void *pValue )
{
    syncGroupReadNotify * pNotify = 0;
    try {
        {
            epicsGuard < epicsMutex > locker ( this->mutex );
            pNotify = syncGroupReadNotify::factory ( 
                this->freeListReadOP, *this, pChan, pValue );
            if ( pNotify ) {
                this->ioPendingList.add ( *pNotify );
            }
            else {
                return ECA_ALLOCMEM;
            }
        }
        pNotify->begin ( type, count );
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        destroyPendingIO ( pNotify );
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        destroyPendingIO ( pNotify );
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        destroyPendingIO ( pNotify );
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        destroyPendingIO ( pNotify );
        return ECA_NOTINSERVICE;
    }
    catch ( std::bad_alloc & )
    {
        destroyPendingIO ( pNotify );
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        destroyPendingIO ( pNotify );
        return ECA_INTERNAL;
    }
}

void CASG::destroyPendingIO ( syncGroupNotify * pNotify )
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    if ( pNotify ) {
        this->ioPendingList.remove ( *pNotify );
        pNotify->destroy ( *this );
    }
}

void CASG::completionNotify ( syncGroupNotify & notify )
{
    unsigned requestsIncomplete;
    {
        epicsGuard < epicsMutex > locker ( this->mutex );
        this->ioPendingList.remove ( notify );
        this->ioCompletedList.add ( notify );
        requestsIncomplete = this->ioPendingList.count ();
    }
    if ( requestsIncomplete == 0u ) {
        this->sem.signal ();
    }
}

void CASG::recycleSyncGroupWriteNotify ( syncGroupWriteNotify &io )
{
    this->freeListWriteOP.release ( &io, sizeof ( io ) );
}

void CASG::recycleSyncGroupReadNotify ( syncGroupReadNotify &io )
{
    this->freeListReadOP.release ( &io, sizeof ( io ) );
}

void * CASG::operator new (size_t size)
{
    return CASG::pFreeList->allocate ( size );
}

void CASG::operator delete (void *pCadaver, size_t size)
{
    CASG::pFreeList->release ( pCadaver, size );
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


 
