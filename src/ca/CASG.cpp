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

#include "iocinf.h"
#include "oldAccess.h"

tsFreeList < struct CASG, 128 > CASG::freeList;
epicsMutex CASG::freeListMutex;

CASG::CASG ( cac &cacIn ) :
    opPendCount ( 0u ),  seqNo ( 0u ), 
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
        {
            epicsAutoMutex locker ( this->mutex );
            tsDLIterBD <syncGroupNotify> notify = this->ioList.firstIter ();
            while ( notify.valid () ) {
                notify->release ();
                notify = this->ioList.firstIter ();
            }
        }
        this->client.uninstallCASG ( *this );
        this->magic = 0;
    }
    else {
        ca_printf ("cac: attempt to destroy invalid sync group ignored\n");
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
    unsigned long initialSeqNo = this->seqNo;
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

    status = ECA_NORMAL;
    while ( 1 ) {
        {
            epicsAutoMutex locker ( this->mutex );
            if ( this->seqNo != initialSeqNo ) {
                break;
            }
            if ( this->opPendCount == 0u ) {
                this->seqNo++;
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
            {
                epicsAutoMutex locker ( this->mutex );
                this->seqNo++;
            }
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
    this->opPendCount = 0;
    this->seqNo++;
}

void CASG::show ( unsigned level) const
{
    printf ( "Sync Group: id=%u, magic=%u, opPend=%lu, seqNo=%lu\n",
        this->getId (), this->magic, this->opPendCount, this->seqNo );

    if ( level ) {
        epicsAutoMutex locker ( this->mutex );
        tsDLIterConstBD < syncGroupNotify > notify = this->ioList.firstIter ();
        while ( notify.valid () ) {
            notify->show (level);
            notify++;
        }
    }
}

bool CASG::ioComplete () const
{
    return ( this->opPendCount == 0u );
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

int CASG::put (chid pChan, unsigned type, unsigned long count, const void *pValue)
{
    syncGroupNotify *pNotify = new syncGroupNotify ( *this, 0);
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }
    int status = pChan->write ( type, count, pValue, *pNotify );
    if ( status != ECA_NORMAL ) {
        pNotify->release ();
    }
    return status;
}

int CASG::get (chid pChan, unsigned type, unsigned long count, void *pValue)
{
    syncGroupNotify *pNotify = new syncGroupNotify ( *this, pValue);
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }
    int status = pChan->read ( type, count, *pNotify );
    if ( status != ECA_NORMAL ) {
        pNotify->release ();
    }
    return status;
}

