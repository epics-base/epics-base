
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
#include "epicsThread.h"
#include "epicsGuard.h"
#include "ipAddrToAsciiAsynchronous.h"

epicsMutex ipAddrToAsciiEngine::mutex;

ipAddrToAsciiEngine::ipAddrToAsciiEngine ( const char *pName ) :
    thread ( * new epicsThread ( *this, pName, 0x2000, epicsThreadPriorityLow ) ),
    pCurrent ( 0 ), exitFlag ( false ), cancelPending ( false ),
    callbackInProgress ( false ), waitingForCancelPendCompletion (false )
{
    this->thread.start (); // start the thread
}

ipAddrToAsciiEngine::~ipAddrToAsciiEngine ()
{
    ipAddrToAsciiAsynchronous * pItem;

    this->exitFlag = true;
    this->laborEvent.signal ();
    this->threadExit.wait ();

    // force IO completion for any items that remain
    {
        epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
        while ( ( pItem = this->labor.first () ) ) {
            pItem->pEngine = 0u;
            sockAddrToA ( &pItem->addr.sa, this->nameTmp, 
                sizeof ( this->nameTmp ) );
            pItem->ioCompletionNotify ( this->nameTmp );
        }
        if ( this->cancelPending ) {
            this->waitingForCancelPendCompletion = true;
        }
    }

    delete & thread;

    while ( this->cancelPending ) {
        this->cancelPendCompleted.wait ();
    }
}

void ipAddrToAsciiEngine::run ()
{
    osiSockAddr addr;

    while ( ! this->exitFlag ) {
        while ( true ) {
            {
                epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
                ipAddrToAsciiAsynchronous * pItem = this->labor.get ();
                if ( pItem ) {
                    addr = pItem->addr;
                    this->pCurrent = pItem;
                }
                else {
                    break;
                }
            }

            // depending on DNS configuration, this could take a very long time
            sockAddrToA ( &addr.sa, this->nameTmp, sizeof ( this->nameTmp ) );

            {
                epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
                if ( this->pCurrent ) {
                    this->pCurrent->pEngine = 0;
                    this->callbackInProgress = true;
                }
                else {
                    continue;
                }
            }

            // dont call callback with lock applied
            this->pCurrent->ioCompletionNotify ( this->nameTmp );

            {
                epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
                this->pCurrent = 0;
                this->callbackInProgress = false;
            }
            if ( this->cancelPending  ) {
                this->destructorBlockEvent.signal ();
            }
        }
        this->laborEvent.wait ();
    }
    this->threadExit.signal ();
}

void ipAddrToAsciiEngine::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
    printf ( "ipAddrToAsciiEngine at %p with %u requests pendingh\n", 
        static_cast <const void *> (this), this->labor.count () );
    if ( level > 0u ) {
        tsDLIterConstBD < ipAddrToAsciiAsynchronous > pItem = this->labor.firstIter ();
        while ( pItem.valid () ) {
            pItem->show ( level - 1u );
            pItem++;
        }
    }
    if ( level > 1u ) {
        printf ( "mutex:\n" );
        this->mutex.show ( level - 2u );
        printf ( "laborEvent:\n" );
        this->laborEvent.show ( level - 2u );
        printf ( "exitFlag  boolean = %u\n", this->exitFlag );
        printf ( "exit event:\n" );
        this->threadExit.show ( level - 2u );
    }
}

ipAddrToAsciiAsynchronous::ipAddrToAsciiAsynchronous 
    ( const osiSockAddr &addrIn ) :
    addr ( addrIn ), pEngine ( 0u )
{
}

ipAddrToAsciiAsynchronous::~ipAddrToAsciiAsynchronous ()
{
    epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
    if ( this->pEngine ) {
        while ( true ) {
            if ( this->pEngine->pCurrent == this && 
                    this->pEngine->callbackInProgress && 
                    ! this->pEngine->thread.isCurrentThread() ) {
                this->pEngine->cancelPending = true;
                {
                    epicsGuardRelease < epicsMutex > unlocker ( locker );
                    this->pEngine->destructorBlockEvent.wait ();
                }
                if ( ! this->pEngine ) {
                    break;
                }
                this->pEngine->cancelPending = false;
                if ( this->pEngine->waitingForCancelPendCompletion ) {
                    this->pEngine->cancelPendCompleted.signal ();
                }
                continue;
            }
            else {
                if ( this->pEngine->pCurrent != this ) {
                    this->pEngine->labor.remove ( *this );
                }
                else {
                    this->pEngine->pCurrent = 0;
                }
                break;
            }
        }
    }
}

epicsShareFunc void ipAddrToAsciiAsynchronous::ioInitiate ( ipAddrToAsciiEngine & engine )
{
    bool success;

    {
        epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
        // put some reasonable limit on queue expansion
        if ( !this->pEngine && engine.labor.count () < 16u ) {
            this->pEngine = & engine;
            engine.labor.add ( *this );
            success = true;
        }
        else {
            success = false;
        }
    }

    if ( success ) {
        engine.laborEvent.signal ();
    }
    else {
        char autoNameTmp[256];
        sockAddrToA ( &this->addr.sa, autoNameTmp, 
            sizeof ( autoNameTmp ) );
        this->ioCompletionNotify ( autoNameTmp );
    }
}

void ipAddrToAsciiAsynchronous::show ( unsigned level ) const
{
    char ipAddr [64];

    epicsGuard < epicsMutex > locker ( ipAddrToAsciiEngine::mutex );
    sockAddrToA ( &this->addr.sa, ipAddr, sizeof ( ipAddr ) );
    printf ( "ipAddrToAsciiAsynchronous for address %s\n", ipAddr );
    if ( level > 0u ) {
        printf ( "\tengine %p\n",
            static_cast <void *> (this->pEngine) );
    }
}
