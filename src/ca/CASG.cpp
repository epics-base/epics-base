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

#include <string>
#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"
#include "cac.h"
#include "sgAutoPtr.h"

casgRecycle::~casgRecycle () {}

CASG::CASG ( epicsGuard < epicsMutex > & guard, ca_client_context & cacIn ) :
    client ( cacIn ), magic ( CASG_MAGIC )
{
    client.installCASG ( guard, *this );
}

CASG::~CASG ()
{
}

void CASG::destructor ( epicsGuard < epicsMutex > & guard )
{
    if ( this->verify ( guard ) ) {
        this->reset ( guard );
        this->client.uninstallCASG ( guard, *this );
        this->magic = 0;
    }
    else {
        this->printf ( "cac: attempt to destroy invalid sync group ignored\n" );
    }
    this->~CASG ();
}

bool CASG::verify ( epicsGuard < epicsMutex > & ) const
{
    return ( this->magic == CASG_MAGIC );
}

/*
 * CASG::block ()
 */
int CASG::block ( epicsGuard < epicsMutex > & guard, double timeout )
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

    this->client.flush ( guard );

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

        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->client.blockForEventAndEnableCallbacks ( 
                this->sem, remaining );
        }

        /*
         * force a time update 
         */
        cur_time = epicsTime::getCurrent ();

        delay = cur_time - beg_time;
    }

    this->reset ( guard );

    return status;
}

void CASG::reset ( 
    epicsGuard < epicsMutex > & guard )
{
    this->destroyCompletedIO ( guard );
    this->destroyPendingIO ( guard );
}

// lock must be applied
void CASG::destroyCompletedIO ( 
    epicsGuard < epicsMutex > & guard )
{
    syncGroupNotify * pNotify;
    while ( ( pNotify = this->ioCompletedList.get () ) ) {
        pNotify->destroy ( guard, * this );
    }
}

// lock must be applied
void CASG::destroyPendingIO ( 
    epicsGuard < epicsMutex > & guard )
{
    syncGroupNotify *pNotify;
    while ( ( pNotify = this->ioPendingList.get () ) ) {
        pNotify->destroy ( guard, * this );
    }
}

void CASG::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->client.mutexRef () );
    this->show ( guard, level );
}

void CASG::show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    ::printf ( "Sync Group: id=%u, magic=%u, opPend=%u\n",
        this->getId (), this->magic, this->ioPendingList.count () );
    if ( level ) {
        ::printf ( "\tPending" );
        tsDLIterConst < syncGroupNotify > notifyPending = 
            this->ioPendingList.firstIter ();
        while ( notifyPending.valid () ) {
            notifyPending->show ( guard, level - 1u );
            notifyPending++;
        }
        ::printf ( "\tCompleted" );
        tsDLIterConst < syncGroupNotify > notifyCompleted = 
            this->ioCompletedList.firstIter ();
        while ( notifyCompleted.valid () ) {
            notifyCompleted->show ( guard, level - 1u );
            notifyCompleted++;
        }
    }
}

bool CASG::ioComplete (
    epicsGuard < epicsMutex > & guard )
{
    this->destroyCompletedIO ( guard );
    return this->ioPendingList.count () == 0u;
}

void CASG::put ( epicsGuard < epicsMutex > & guard, chid pChan, 
    unsigned type, arrayElementCount count, const void * pValue )
{
    sgAutoPtr < syncGroupWriteNotify > pNotify ( guard, *this );
    {
        pNotify = syncGroupWriteNotify::factory ( 
            this->freeListWriteOP, *this, pChan );
        if ( pNotify.get () ) {
            this->ioPendingList.add ( *pNotify );
        }
        else {
            return;
        }
    }
    pNotify->begin ( guard, type, count, pValue );
    pNotify.release ();
}

void CASG::get ( epicsGuard < epicsMutex > & guard, chid pChan, 
                unsigned type, arrayElementCount count, void *pValue )
{
    sgAutoPtr < syncGroupReadNotify > pNotify ( guard, *this );
    {
        pNotify = syncGroupReadNotify::factory ( 
            this->freeListReadOP, *this, pChan, pValue );
        if ( pNotify.get () ) {
            this->ioPendingList.add ( *pNotify );
        }
        else {
            return;
        }
    }
    pNotify->begin ( guard, type, count );
    pNotify.release ();
}

void CASG::destroyPendingIO ( 
    epicsGuard < epicsMutex > & guard, syncGroupNotify * pNotify )
{
    if ( pNotify ) {
        this->ioPendingList.remove ( *pNotify );
        pNotify->destroy ( guard, *this );
    }
}

void CASG::completionNotify ( 
    epicsGuard < epicsMutex > & guard, syncGroupNotify & notify )
{
    this->ioPendingList.remove ( notify );
    this->ioCompletedList.add ( notify );
    if ( this->ioPendingList.count () == 0u ) {
        this->sem.signal ();
    }
}

void CASG::recycleSyncGroupWriteNotify ( 
    epicsGuard < epicsMutex > & guard, syncGroupWriteNotify & io )
{
    this->freeListWriteOP.release ( & io );
}

void CASG::recycleSyncGroupReadNotify ( 
    epicsGuard < epicsMutex > & guard, syncGroupReadNotify & io )
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

void CASG::exception ( 
    epicsGuard < epicsMutex > & guard, 
    int status, const char * pContext, 
    const char * pFileName, unsigned lineNo )
{
    if ( status != ECA_CHANDESTROY ) {
        this->client.exception ( 
            guard, status, pContext, pFileName, lineNo );
    }
}

void CASG::exception ( 
    epicsGuard < epicsMutex > & guard, 
    int status, const char * pContext,
    const char * pFileName, unsigned lineNo, oldChannelNotify & chan, 
    unsigned type, arrayElementCount count, unsigned op )
{
    if ( status != ECA_CHANDESTROY ) {
        this->client.exception ( 
            guard, status, pContext, pFileName, 
            lineNo, chan, type, count, op );
    }
}

void * CASG::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void CASG::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}


 
