
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

#include <iocinf.h>

recvProcessThread::recvProcessThread (cac *pcacIn) :
    osiThread ( "CAC-recv-process", threadGetStackSize (threadStackSmall), threadPriorityMedium ), 
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

        this->mutex.lock ();
        if ( this->enableRefCount ) {
            this->processing = true;
        }
        this->mutex.unlock ();

        if ( this->processing ) {
            pcac->processRecvBacklog ();
        }

        this->processing = false;
        if ( this->blockingForCompletion ) {
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

    this->mutex.lock ();
    assert ( this->enableRefCount < UINT_MAX );
    copy = this->enableRefCount;
    this->enableRefCount++;
    this->mutex.unlock ();
    
    if ( copy == 0u ) {
        this->recvActivity.signal ();
    }
}

void recvProcessThread::disable ()
{
    bool waitNeeded;

    this->mutex.lock ();
    assert ( this->enableRefCount != 0u );
    this->enableRefCount--;
    if ( this->enableRefCount == 0u && this->processing ) {
        waitNeeded = true;
        this->blockingForCompletion++;
    }
    else {
        waitNeeded = false;
    }
    this->mutex.unlock ();

    if ( waitNeeded ) {
        while ( this->processing ) {
            this->processingDone.wait ();
        }
        this->mutex.lock ();
        this->blockingForCompletion--;
        this->mutex.unlock ();
        if ( this->blockingForCompletion ) {
            this->processingDone.signal ();
        }
    }
}

void recvProcessThread::signalActivity ()
{
    this->recvActivity.signal ();
}

void recvProcessThread::show ( unsigned level ) const
{
    this->mutex.lock ();
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
    this->mutex.unlock ();
}
