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

#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"
#include "autoPtrDestroy.h"
#include "cac.h"

tsFreeList < struct CASG, 128 > CASG::freeList;
epicsMutex CASG::freeListMutex;

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
    double  delay;
    double  remaining;
    int     status;

    if ( timeout < 0.0 ) {
        return ECA_TIMEOUT;
    }

    cur_time = epicsTime::getCurrent ();

    this->client.flushRequest ();

    beg_time = cur_time;
    delay = 0.0;

    this->client.enableCallbackPreemption ();

    while ( 1 ) {
        {
            epicsAutoMutex locker ( this->mutex );
            if ( this->ioList.count() == 0u ) {
                status = ECA_NORMAL;
                break;
            }
        }

        remaining = timeout - delay;
        if ( remaining <= CAC_SIGNIFICANT_SELECT_DELAY ) {
            /*
             * Make sure that we take care of
             * recv backlog at least once
             */
            status = ECA_TIMEOUT;
            this->reset ();
            break;
        }

        this->sem.wait ( remaining );

        /*
         * force a time update 
         */
        cur_time = epicsTime::getCurrent ();

        delay = cur_time - beg_time;
    }

    this->client.disableCallbackPreemption ();

    return status;
}

void CASG::reset ()
{
    epicsAutoMutex locker ( this->mutex );
    syncGroupNotify *pNotify;
    while ( ( pNotify = this->ioList.get () ) ) {
        pNotify->destroy ( * this );
    }
}

void CASG::show ( unsigned level ) const
{
    ::printf ( "Sync Group: id=%u, magic=%u, opPend=%u\n",
        this->getId (), this->magic, this->ioList.count () );
    if ( level ) {
        epicsAutoMutex locker ( this->mutex );
        tsDLIterConstBD < syncGroupNotify > notify = this->ioList.firstIter ();
        while ( notify.valid () ) {
            notify->show ( level - 1u );
            notify++;
        }
    }
}

bool CASG::ioComplete () const
{
    return ( this->ioList.count () == 0u );
}

int CASG::put ( chid pChan, unsigned type, unsigned long count, const void *pValue )
{
    try {
        epicsAutoMutex locker ( this->mutex );
        syncGroupNotify *pNotify = syncGroupWriteNotify::factory ( 
            this->freeListWriteOP, *this, pChan, type, count, pValue );
        if ( pNotify ) {
            this->ioList.add ( *pNotify );
            return ECA_NORMAL;
        }
        else {
            return ECA_ALLOCMEM;
        }
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        return ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::noMemory & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}

int CASG::get ( chid pChan, unsigned type, unsigned long count, void *pValue )
{

    try {
        epicsAutoMutex locker ( this->mutex );
        syncGroupNotify * pNotify = syncGroupReadNotify::factory ( 
            this->freeListReadOP, *this, pChan, type, count, pValue );
        if ( pNotify ) {
            this->ioList.add ( *pNotify );
            return ECA_NORMAL;
        }
        else {
            return ECA_ALLOCMEM;
        }
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::noMemory & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}

void CASG::destroyIO ( syncGroupNotify &notify )
{
    unsigned requestsIncomplete;
    {
        epicsAutoMutex locker ( this->mutex );
        this->ioList.remove ( notify );
        requestsIncomplete = this->ioList.count ();
        notify.destroy ( *this );
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
    epicsAutoMutex locker ( CASG::freeListMutex );
    return CASG::freeList.allocate ( size );
}

void CASG::operator delete (void *pCadaver, size_t size)
{
    epicsAutoMutex locker ( CASG::freeListMutex );
    CASG::freeList.release ( pCadaver, size );
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


