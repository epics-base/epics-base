
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
#include "ipAddrToAsciiAsynchronous.h"

epicsMutex ipAddrToAsciiEngine::mutex;

ipAddrToAsciiEngine::ipAddrToAsciiEngine ( const char *pName ) :
    thread ( * new epicsThread ( *this, pName, 0x1000, epicsThreadPriorityLow ) ),
    nextId ( 0u ), exitFlag ( false )
{
    this->thread.start (); // start the thread
}

ipAddrToAsciiEngine::~ipAddrToAsciiEngine ()
{
    ipAddrToAsciiAsynchronous * pItem;

    this->exitFlag = true;
    this->event.signal ();
    this->threadExit.wait ();

    // force IO completion for any items that remain
    {
        epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
        while ( ( pItem = this->labor.first () ) ) {
            pItem->pEngine = 0u;
            sockAddrToA ( &pItem->addr.sa, this->nameTmp, 
                sizeof ( this->nameTmp ) );
            pItem->ioCompletionNotify ( this->nameTmp );
        }
    }
    delete &thread;
}

void ipAddrToAsciiEngine::run ()
{
    osiSockAddr addr;
    unsigned tmpId;

    while ( ! this->exitFlag ) {
        while ( true ) {
            {
                epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
                ipAddrToAsciiAsynchronous * pItem = this->labor.first ();
                if ( pItem ) {
                    addr = pItem->addr;
                    tmpId = pItem->id;
                }
                else {
                    break;
                }
            }

            // knowing DNS, this could take a very long time
            sockAddrToA ( &addr.sa, this->nameTmp, sizeof ( this->nameTmp ) );

            {
                epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
                ipAddrToAsciiAsynchronous * pItem = this->labor.get ();
                if ( pItem ) {
                    if ( tmpId == pItem->id ) {
                        pItem->ioCompletionNotify ( this->nameTmp );
                        pItem->pEngine = 0u;
                    }
                    else {
                        this->labor.push ( *pItem );
                    }
                }
                else {
                    break;
                }
            }
        }
        this->event.wait ();
    }
    this->threadExit.signal ();
}

void ipAddrToAsciiEngine::show ( unsigned level ) const
{
    epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
    printf ( "ipAddrToAsciiEngine at %p with %u requests pendingh\n", 
        static_cast <const void *> (this), this->labor.count () );
    if ( level > 0u ) {
        tsDLIterConstBD < ipAddrToAsciiAsynchronous > pItem = this->labor.firstIter ();
        while ( pItem.valid () ) {
            pItem->show ( level - 1u );
            pItem++;
        }
        printf ( "nextId = %u\n", this->nextId );
    }
    if ( level > 1u ) {
        printf ( "mutex:\n" );
        this->mutex.show ( level - 2u );
        printf ( "event:\n" );
        this->event.show ( level - 2u );
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
    epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
    if ( this->pEngine )  {
        this->pEngine->labor.remove ( *this );
    }
}

epicsShareFunc bool ipAddrToAsciiAsynchronous::ioInitiate ( ipAddrToAsciiEngine &engine )
{
    bool success;

    {
        epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
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
    }

    if ( success ) {
        engine.event.signal ();
    }

    return success;
}

void ipAddrToAsciiAsynchronous::show ( unsigned level ) const
{
    char ipAddr [64];

    epicsAutoMutex locker ( ipAddrToAsciiEngine::mutex );
    sockAddrToA ( &this->addr.sa, ipAddr, sizeof ( ipAddr ) );
    printf ( "ipAddrToAsciiAsynchronous for address %s\n", ipAddr );
    if ( level > 0u ) {
        printf ( "\tidentifier %u, engine %p\n", this->id, 
            static_cast <void *> (this->pEngine) );
    }
}
