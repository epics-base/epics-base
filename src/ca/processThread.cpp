
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
 */

#include <iocinf.h>

processThread::processThread (cac *pcacIn) :
    osiThread ("CAC process", 0x1000, threadPriorityMedium), 
    pcac (pcacIn),
    shutDown (false)
{
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
        pcac->processRecvBacklog ();
        this->pcac->recvActivity.wait ();
    }
    this->exit.signal ();
}

void processThread::signalShutDown ()
{
    this->shutDown = true;
    this->pcac->recvActivity.signal ();
}

