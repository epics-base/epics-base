
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

processThread::processThread () :
    osiThread ("CAC process", 0x1000, threadPriorityMedium), 
    shutDown (false)
{
	ellInit (&this->recvActivity);
}

processThread::~processThread ()
{
    this->shutDown = true;
    this->exit.signal ();
    while ( !this->exit.wait (5.0) ) {
        printf ("processThread::~processThread (): Warning, thread object destroyed before thread exit \n");
    }
}

void processThread::entryPoint ()
{
    char *pNode;
    tcpiiu *piiu;

    while (!this->shutDown) {
        while ( 1 ) {
            int status;
            unsigned bytesToProcess;

            this->mutex.lock ();
            pNode = (char *) ellGet (&this->recvActivity);
            if (pNode) {
                piiu = (tcpiiu *) (pNode - offsetof (tcpiiu, recvActivityNode) );
                piiu->recvPending = FALSE;
            }
            this->mutex.unlock ();

            if (!pNode) {
                break;
            }

            if ( piiu->state == iiu_connected ) {
                char *pProto = (char *) cacRingBufferReadReserveNoBlock 
                                    (&piiu->recv, &bytesToProcess);
                if (pProto) {
                    status = post_msg (&piiu->niiu, &piiu->dest.ia, 
                                pProto, bytesToProcess);
                    if (status!=ECA_NORMAL) {
                        initiateShutdownTCPIIU (piiu);
                    }
                    cacRingBufferReadCommit (&piiu->recv, bytesToProcess);
                    cacRingBufferReadFlush (&piiu->recv);
                }
            }
        }

        this->wakeup.wait ();
    }
    this->exit.signal ();
}

void processThread::signalShutDown ()
{
    this->shutDown = true;
    this->wakeup.signal ();
}

void processThread::installLabor (tcpiiu &iiu)
{
    bool addedIt;

    this->mutex.lock ();
    if ( !iiu.recvPending ) {
        iiu.recvPending = TRUE;
	    ellAdd (&this->recvActivity, &iiu.recvActivityNode);
        addedIt = true;
    }
    else {
        addedIt = false;
    }
    this->mutex.unlock ();

    //
    // wakeup after unlock improves performance
    //
    if (addedIt) {
        this->wakeup.signal ();
    }
}

void processThread::removeLabor (tcpiiu &iiu)
{
    this->mutex.lock ();
    if (iiu.recvPending) {
        ellDelete (&this->recvActivity, &iiu.recvActivityNode);
    }
    this->mutex.unlock ();
}
