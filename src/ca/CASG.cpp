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

CASG::CASG (cac &cacIn) :
    client (cacIn), magic (CASG_MAGIC), opPendCount (0u), seqNo (0u)
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
        this->mutex.lock ();
        tsDLIterBD <syncGroupNotify> notify ( this->ioList.first () );
        while ( notify != notify.eol () ) {
            notify->destroy ();
            notify = this->ioList.first ();
        }
        this->mutex.unlock ();
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
    osiTime cur_time;
    osiTime beg_time;
    double  delay;
    double  remaining;
    int     status;

    if ( timeout < 0.0 ) {
        return ECA_TIMEOUT;
    }

    /*
     * dont allow recursion
     */
    void *p = threadPrivateGet (cacRecursionLock);
    if ( p ) {
        return ECA_EVDISALLOW;
    }

    threadPrivateSet (cacRecursionLock, &cacRecursionLock);

    cur_time = osiTime::getCurrent ();

    this->client.flush ();

    beg_time = cur_time;
    delay = 0.0;

    this->client.enableCallbackPreemption ();

    status = ECA_NORMAL;
    while ( 1 ) {
        this->mutex.lock ();
        if ( this->seqNo != initialSeqNo ) {
            this->mutex.unlock ();
            break;
        }
        if ( this->opPendCount == 0u ) {
            this->seqNo++;
            this->mutex.unlock ();
            break;
        }
        this->mutex.unlock ();

        /*
         * Exit if the timeout has expired
         * (dont wait forever for an itsy bitsy
         * delay which will not be updated if
         * select is called with no delay)
         *
         * current time is only updated by
         * cac_select_io() if we specify
         * at non-zero delay
         */
        remaining = timeout - delay;
        if ( remaining <= CAC_SIGNIFICANT_SELECT_DELAY ) {
            /*
             * Make sure that we take care of
             * recv backlog at least once
             */
            status = ECA_TIMEOUT;
            this->mutex.lock ();
            this->seqNo++;
            this->mutex.unlock ();
            break;
        }

        this->sem.wait ( remaining );

        /*
         * force a time update 
         */
        cur_time = osiTime::getCurrent ();

        delay = cur_time - beg_time;
    }

    this->client.disableCallbackPreemption ();

    threadPrivateSet (cacRecursionLock, NULL);

    return status;
}

void CASG::reset ()
{
    this->mutex.lock ();
    this->opPendCount = 0;
    this->seqNo++;
    this->mutex.unlock ();
}

void CASG::show ( unsigned level) const
{
    printf ("Sync Group: id=%u, magic=%u, opPend=%lu, seqNo=%lu\n",
        this->getId (), this->magic, this->opPendCount, this->seqNo);

    if ( level ) {
        this->mutex.lock ();
        tsDLIterBD <syncGroupNotify> notify ( this->ioList.first () );
        while ( notify != notify.eol () ) {
            notify->show (level);
        }
        this->mutex.unlock ();
    }
}

bool CASG::ioComplete () const
{
    return ( this->opPendCount == 0u );
}

void * CASG::operator new (size_t size)
{
    return CASG::freeList.allocate ( size );
}

void CASG::operator delete (void *pCadaver, size_t size)
{
    CASG::freeList.release ( pCadaver, size );
}

int CASG::put (chid pChan, unsigned type, unsigned long count, const void *pValue)
{
    syncGroupNotify *pNotify = new syncGroupNotify ( *this, 0);
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }
    return pChan->write ( type, count, pValue, *pNotify );
}

int CASG::get (chid pChan, unsigned type, unsigned long count, void *pValue)
{
    syncGroupNotify *pNotify = new syncGroupNotify ( *this, pValue);
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }
    return pChan->read ( type, count, *pNotify );
}

