
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
 */

#define epicsExportSharedSymbols
#include "osiThread.h"
#include "ipAddrToAsciiAsynchronous.h"

osiMutex ipAddrToAsciiEngine::mutex;

ipAddrToAsciiEngine::ipAddrToAsciiEngine ( const char *pName ) :
    osiThread ( pName, 0x1000, threadPriorityLow ), nextId ( 0u ),
    exitFlag ( false )
{
    this->start (); // start the thread
}

ipAddrToAsciiEngine::~ipAddrToAsciiEngine ()
{
    ipAddrToAsciiAsynchronous * pItem = this->labor.first ();

    this->event.signal ();
    this->exitFlag = true;
    this->threadExit.wait ();

    // force IO completion for any items that remain
    ipAddrToAsciiEngine::mutex.lock ();
    while ( ( pItem = this->labor.first () ) ) {
        pItem->pEngine = 0u;
        sockAddrToA ( &pItem->addr.sa, this->nameTmp, 
            sizeof ( this->nameTmp ) );
        pItem->ioCompletionNotify ( this->nameTmp );
    }
    ipAddrToAsciiEngine::mutex.unlock ();
}

void ipAddrToAsciiEngine::entryPoint ()
{
    osiSockAddr addr;
    unsigned id;

    while ( ! this->exitFlag ) {
        while ( true ) {
            ipAddrToAsciiEngine::mutex.lock ();
            ipAddrToAsciiAsynchronous * pItem = this->labor.first ();
            if ( pItem ) {
                addr = pItem->addr;
                id = pItem->id;
            }
            else {
                this->mutex.unlock ();
                break;
            }
            ipAddrToAsciiEngine::mutex.unlock ();

            // knowing DNS, this could take a very long time
            sockAddrToA ( &addr.sa, this->nameTmp, sizeof ( this->nameTmp ) );

            ipAddrToAsciiEngine::mutex.lock ();
            pItem = this->labor.get ();
            if ( pItem ) {
                if ( id == pItem->id ) {
                    pItem->ioCompletionNotify ( this->nameTmp );
                    pItem->pEngine = 0u;
                }
                else {
                    this->labor.push ( *pItem );
                }
            }
            else {
                ipAddrToAsciiEngine::mutex.unlock ();
                break;
            }
            ipAddrToAsciiEngine::mutex.unlock ();
        }
        this->event.wait ();
    }
    this->threadExit.signal ();
}

ipAddrToAsciiAsynchronous::ipAddrToAsciiAsynchronous 
    ( const osiSockAddr &addrIn ) :
    addr ( addrIn ), pEngine ( 0u )
{
}

ipAddrToAsciiAsynchronous::~ipAddrToAsciiAsynchronous ()
{
    ipAddrToAsciiEngine::mutex.lock ();
    if ( this->pEngine )  {
        this->pEngine->labor.remove ( *this );
    }
    ipAddrToAsciiEngine::mutex.unlock ();
}

epicsShareFunc bool ipAddrToAsciiAsynchronous::ioInitiate ( ipAddrToAsciiEngine &engine )
{
    bool success;

    ipAddrToAsciiEngine::mutex.lock ();
    // put some reasonable limit on queue expansion
    if ( engine.labor.count () < 16u ) {
        this->id = engine.nextId++;
        this->pEngine = &engine;
        engine.labor.add ( *this );
        success = true;
    }
    else {
        success = false;
    }
    ipAddrToAsciiEngine::mutex.unlock ();

    if ( success ) {
        engine.event.signal ();
    }

    return success;
}
