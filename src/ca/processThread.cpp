
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

processThread::processThread (cac *pcacIn) :
    osiThread ( "CAC-process", threadGetStackSize (threadStackSmall), threadPriorityMedium ), 
    pcac ( pcacIn ),
    enableRefCount ( 0u ),
    blockingForCompletion ( 0u ),
    processing ( false ),
    shutDown ( false )
{
    this->start ();
}

processThread::~processThread ()
{
    this->signalShutDown ();
    while ( ! this->exit.wait ( 10.0 ) ) {
        printf ("processThread::~processThread (): Warning, thread object destroyed before thread exit \n");
    }
}

void processThread::entryPoint ()
{
    int status = ca_attach_context ( this->pcac );
    SEVCHK ( status, "attaching to client context in process thread" );
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

        this->pcac->recvActivity.wait ();
    }
    this->exit.signal ();
}

void processThread::signalShutDown ()
{
    this->shutDown = true;
    this->pcac->recvActivity.signal ();
}

void processThread::enable ()
{
    unsigned copy;

    this->mutex.lock ();
    assert ( this->enableRefCount < UINT_MAX );
    copy = this->enableRefCount;
    this->enableRefCount++;
    this->mutex.unlock ();
    
    if ( copy == 0u ) {
        this->pcac->recvActivity.signal ();
    }
}

void processThread::disable ()
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