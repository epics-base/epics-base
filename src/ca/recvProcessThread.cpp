
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 *
 */

#include "iocinf.h"
#include "cac_IL.h"

recvProcessThread::recvProcessThread (cac *pcacIn) :
    osiThread ( "CAC-recv-process", threadGetStackSize (threadStackSmall), 
        pcacIn->getInitializingThreadsPriority () ), 
    pcac ( pcacIn ),
    enableRefCount ( 0u ),
    blockingForCompletion ( 0u ),
    processing ( false ),
    shutDown ( false )
{
    this->start ();
}

recvProcessThread::~recvProcessThread ()
{
    this->signalShutDown ();
    this->exit.wait ();
}

void recvProcessThread::entryPoint ()
{
    int status = ca_attach_context ( this->pcac );
    SEVCHK ( status, "attaching to client context in recv process thread" );
    while ( ! this->shutDown ) {

        {
            epicsAutoMutex autoMutex ( this->mutex );
            if ( this->enableRefCount ) {
                this->processing = true;
            }
        }

        if ( this->processing ) {
            pcac->processRecvBacklog ();
        }

        bool signalNeeded;
        {
            epicsAutoMutex autoMutex ( this->mutex );
            this->processing = false;
            signalNeeded = this->blockingForCompletion > 0u;
        }

        if ( signalNeeded ) {
            this->processingDone.signal ();
        }

        this->recvActivity.wait ();
    }
    this->exit.signal ();
}

void recvProcessThread::signalShutDown ()
{
    this->shutDown = true;
    this->recvActivity.signal ();
}

void recvProcessThread::enable ()
{
    unsigned copy;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        assert ( this->enableRefCount < UINT_MAX );
        copy = this->enableRefCount;
        this->enableRefCount++;
    }
    
    if ( copy == 0u ) {
        this->recvActivity.signal ();
    }
}

void recvProcessThread::disable ()
{
    bool wakeupNeeded;

    {
        epicsAutoMutex autoMutex ( this->mutex );

        if ( ! this->processing ) {
            assert ( this->enableRefCount != 0u );
            this->enableRefCount--;
            return;
        }
        else {
            this->blockingForCompletion++;
        }
    }

    while ( true ) {
        this->processingDone.wait ();

        {
            epicsAutoMutex autoMutex ( this->mutex );

            if ( ! this->processing ) {
                assert ( this->enableRefCount > 0u );
                this->enableRefCount--;
                assert ( this->blockingForCompletion > 0u );
                this->blockingForCompletion--;
                wakeupNeeded = this->blockingForCompletion > 0u;
                break;
            }
        }
    }

    if ( wakeupNeeded ) {
        this->processingDone.signal ();
    }
}

void recvProcessThread::signalActivity ()
{
    this->recvActivity.signal ();
}

void recvProcessThread::show ( unsigned level ) const
{
    epicsAutoMutex autoMutex ( this->mutex );
    printf ( "CA receive processing thread at %p state=%s\n", 
        this,  this->processing ? "busy" : "idle");
    if ( level > 0u ) {
        printf ( "enable count %u\n", this->enableRefCount );
        printf ( "blocking for completion count %u\n", this->blockingForCompletion );
    }
    if ( level > 1u ) {
        printf ( "\tCA client at %p\n", this->pcac );
        printf ( "\tshutdown command boolean %u\n", this->shutDown );
    }
    if ( level > 2u ) {
        printf ( "Receive activity event:\n" );
        this->recvActivity.show ( level - 3u );
        printf ( "exit event:\n" );
        this->exit.show ( level - 3u );
        printf ( "processing done event:\n" );
        this->processingDone.show ( level - 3u );
        printf ( "mutex:\n" );
        this->mutex.show ( level - 3u );
    }
}
