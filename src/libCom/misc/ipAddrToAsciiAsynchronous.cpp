/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 */

#include <string>
#include <climits>
#include <stdexcept>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "ipAddrToAsciiAsynchronous.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsGuard.h"
#include "epicsExit.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "errlog.h"

// - this class implements the asynchronous DNS query
// - it completes early with the host name in dotted IP address form 
//   if the ipAddrToAsciiEngine is destroyed before IO completion
//   or if there are too many items already in the engine's queue.
class ipAddrToAsciiTransactionPrivate : 
    public ipAddrToAsciiTransaction,
    public tsDLNode < ipAddrToAsciiTransactionPrivate > {
public:
    ipAddrToAsciiTransactionPrivate ( class ipAddrToAsciiEnginePrivate & engineIn );
    virtual ~ipAddrToAsciiTransactionPrivate ();
    osiSockAddr address () const;
    void show ( unsigned level ) const; 
    void * operator new ( size_t size, tsFreeList 
        < ipAddrToAsciiTransactionPrivate, 0x80 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList 
        < ipAddrToAsciiTransactionPrivate, 0x80 > & ))
private:
    osiSockAddr addr;
    ipAddrToAsciiEnginePrivate & engine;
    ipAddrToAsciiCallBack * pCB;
    bool pending;
    void ipAddrToAscii ( const osiSockAddr &, ipAddrToAsciiCallBack & );
    void release (); 
    void * operator new ( size_t ); 
    void operator delete ( void * );
    friend class ipAddrToAsciiEnginePrivate;
    ipAddrToAsciiTransactionPrivate & operator = ( const ipAddrToAsciiTransactionPrivate & );
    ipAddrToAsciiTransactionPrivate ( const ipAddrToAsciiTransactionPrivate & );
};

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class tsFreeList 
    < ipAddrToAsciiTransactionPrivate, 0x80 >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

extern "C" {
static void ipAddrToAsciiEngineGlobalMutexConstruct ( void * );
}

// - this class executes the synchronous DNS query
// - it creates one thread
class ipAddrToAsciiEnginePrivate : 
    public ipAddrToAsciiEngine, 
    public epicsThreadRunable {
public:
    ipAddrToAsciiEnginePrivate ();
    virtual ~ipAddrToAsciiEnginePrivate ();
    void show ( unsigned level ) const; 
private:
    char nameTmp [1024];
    tsFreeList 
        < ipAddrToAsciiTransactionPrivate, 0x80 > 
            transactionFreeList;
    tsDLList < ipAddrToAsciiTransactionPrivate > labor;
    mutable epicsMutex mutex;
    epicsEvent laborEvent;
    epicsEvent destructorBlockEvent;
    epicsThread thread;
    ipAddrToAsciiTransactionPrivate * pCurrent;
    unsigned cancelPendingCount;
    bool exitFlag;
    bool callbackInProgress;
    static ipAddrToAsciiEnginePrivate * pEngine;
    ipAddrToAsciiTransaction & createTransaction ();
    void release (); 
    void run ();
	ipAddrToAsciiEnginePrivate ( const ipAddrToAsciiEngine & );
	ipAddrToAsciiEnginePrivate & operator = ( const ipAddrToAsciiEngine & );
    friend class ipAddrToAsciiEngine;
    friend class ipAddrToAsciiTransactionPrivate;
    friend void ipAddrToAsciiEngineGlobalMutexConstruct ( void * );
};

ipAddrToAsciiEnginePrivate * ipAddrToAsciiEnginePrivate :: pEngine = 0;
static epicsThreadOnceId ipAddrToAsciiEngineGlobalMutexOnceFlag = EPICS_THREAD_ONCE_INIT;

// the users are not required to supply a show routine
// for there transaction callback class
void ipAddrToAsciiCallBack::show ( unsigned /* level */ ) const {}

// some noop pure virtual destructures
ipAddrToAsciiCallBack::~ipAddrToAsciiCallBack () {}
ipAddrToAsciiTransaction::~ipAddrToAsciiTransaction () {}
ipAddrToAsciiEngine::~ipAddrToAsciiEngine () {}

static void ipAddrToAsciiEngineGlobalMutexConstruct ( void * )
{
    try{
        ipAddrToAsciiEnginePrivate::pEngine = new ipAddrToAsciiEnginePrivate ();
    }catch(std::exception& e){
        errlogPrintf("ipAddrToAsciiEnginePrivate ctor fails with: %s\n", e.what());
    }
}

// for now its probably sufficent to allocate one 
// DNS transaction thread for all codes sharing
// the same process that need DNS services but we 
// leave our options open for the future
ipAddrToAsciiEngine & ipAddrToAsciiEngine::allocate ()
{
    epicsThreadOnce (
        & ipAddrToAsciiEngineGlobalMutexOnceFlag,
        ipAddrToAsciiEngineGlobalMutexConstruct, 0 );
    if(!ipAddrToAsciiEnginePrivate::pEngine)
        throw std::runtime_error("ipAddrToAsciiEngine::allocate fails");
    return * ipAddrToAsciiEnginePrivate::pEngine;
}

ipAddrToAsciiEnginePrivate::ipAddrToAsciiEnginePrivate () :
    thread ( *this, "ipToAsciiProxy",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow ),
    pCurrent ( 0 ), cancelPendingCount ( 0u ), exitFlag ( false ),  
    callbackInProgress ( false )
{
    this->thread.start (); // start the thread
}

ipAddrToAsciiEnginePrivate::~ipAddrToAsciiEnginePrivate ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->exitFlag = true;
    }
    this->laborEvent.signal ();
    this->thread.exitWait ();
}

void ipAddrToAsciiEnginePrivate::release ()
{
}

void ipAddrToAsciiEnginePrivate::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    printf ( "ipAddrToAsciiEngine at %p with %u requests pending\n", 
        static_cast <const void *> (this), this->labor.count () );
    if ( level > 0u ) {
        tsDLIterConst < ipAddrToAsciiTransactionPrivate > 
            pItem = this->labor.firstIter ();
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
    }
}

inline void * ipAddrToAsciiTransactionPrivate::operator new ( size_t size, tsFreeList 
    < ipAddrToAsciiTransactionPrivate, 0x80 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void ipAddrToAsciiTransactionPrivate::operator delete ( void * pTrans, tsFreeList 
    < ipAddrToAsciiTransactionPrivate, 0x80 > &  freeList )
{
    freeList.release ( pTrans );
}
#endif

void * ipAddrToAsciiTransactionPrivate::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void ipAddrToAsciiTransactionPrivate::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}


ipAddrToAsciiTransaction & ipAddrToAsciiEnginePrivate::createTransaction ()
{
    return * new ( this->transactionFreeList ) ipAddrToAsciiTransactionPrivate ( *this );
}

void ipAddrToAsciiEnginePrivate::run ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    while ( ! this->exitFlag ) {
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->laborEvent.wait ();
        }
        while ( true ) {
            ipAddrToAsciiTransactionPrivate * pItem = this->labor.get ();
            if ( ! pItem ) {
                break;
            }
            osiSockAddr addr = pItem->addr;
            this->pCurrent = pItem;
    
            if ( this->exitFlag )
            {
                sockAddrToDottedIP ( & addr.sa, this->nameTmp, 
                    sizeof ( this->nameTmp ) );
            }
            else {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                // depending on DNS configuration, this could take a very long time
                // so we release the lock
                sockAddrToA ( &addr.sa, this->nameTmp, sizeof ( this->nameTmp ) );
            }

            // the ipAddrToAsciiTransactionPrivate destructor is allowed to
            // set pCurrent to nill and avoid blocking on a slow DNS 
            // operation
            if ( ! this->pCurrent ) {
                continue;
            }

            // fix for lp:1580623
            // a destructing cac sets pCurrent to NULL, so
            // make local copy to avoid race when releasing the guard
            ipAddrToAsciiTransactionPrivate *pCur = this->pCurrent;
            this->callbackInProgress = true;

            {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                // dont call callback with lock applied
                pCur->pCB->transactionComplete ( this->nameTmp );
            }

            this->callbackInProgress = false;

            if ( this->pCurrent ) {
                this->pCurrent->pending = false;
                this->pCurrent = 0;
            }
            if ( this->cancelPendingCount  ) {
                this->destructorBlockEvent.signal ();
            }
        }
    }
}

ipAddrToAsciiTransactionPrivate::ipAddrToAsciiTransactionPrivate 
    ( ipAddrToAsciiEnginePrivate & engineIn ) :
    engine ( engineIn ), pCB ( 0 ), pending ( false )
{
    memset ( & this->addr, '\0', sizeof ( this->addr ) );
    this->addr.sa.sa_family = AF_UNSPEC;
}

void ipAddrToAsciiTransactionPrivate::release ()
{
    this->~ipAddrToAsciiTransactionPrivate ();
    this->engine.transactionFreeList.release ( this );
}

ipAddrToAsciiTransactionPrivate::~ipAddrToAsciiTransactionPrivate ()
{
    epicsGuard < epicsMutex > guard ( this->engine.mutex );
    while ( this->pending ) {
        if ( this->engine.pCurrent == this && 
                this->engine.callbackInProgress && 
                ! this->engine.thread.isCurrentThread() ) {
            // cancel from another thread while callback in progress
            // waits for callback to complete
            assert ( this->engine.cancelPendingCount < UINT_MAX );
            this->engine.cancelPendingCount++;
            {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                this->engine.destructorBlockEvent.wait ();
            }
            assert ( this->engine.cancelPendingCount > 0u );
            this->engine.cancelPendingCount--;
            if ( ! this->pending ) {
                if ( this->engine.cancelPendingCount ) {
                    this->engine.destructorBlockEvent.signal ();
                }
                break;
            }
        }
        else {
            if ( this->engine.pCurrent == this ) {
                // cancel from callback, or while lookup in progress
                this->engine.pCurrent = 0;
            }
            else {
                // cancel before lookup starts
                this->engine.labor.remove ( *this );
            }
            this->pending = false;
        }
    }
}

void ipAddrToAsciiTransactionPrivate::ipAddrToAscii ( 
    const osiSockAddr & addrIn, ipAddrToAsciiCallBack & cbIn )
{
    bool success;

    {
        epicsGuard < epicsMutex > guard ( this->engine.mutex );
        // put some reasonable limit on queue expansion
        if ( !this->pending && engine.labor.count () < 16u ) {
            this->addr = addrIn;
            this->pCB = & cbIn;
            this->pending = true;
            this->engine.labor.add ( *this );
            success = true;
        }
        else {
            success = false;
        }
    }

    if ( success ) {
        this->engine.laborEvent.signal ();
    }
    else {
        char autoNameTmp[256];
        sockAddrToDottedIP ( & addrIn.sa, autoNameTmp, 
            sizeof ( autoNameTmp ) );
        cbIn.transactionComplete ( autoNameTmp );
    }
}

osiSockAddr ipAddrToAsciiTransactionPrivate::address () const
{
    return this->addr;
}

void ipAddrToAsciiTransactionPrivate::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->engine.mutex );
    char ipAddr [64];
    sockAddrToDottedIP ( &this->addr.sa, ipAddr, sizeof ( ipAddr ) );
    printf ( "ipAddrToAsciiTransactionPrivate for address %s\n", ipAddr );
    if ( level > 0u ) {
        printf ( "\tengine %p\n",
            static_cast <void *> ( & this->engine ) );
        this->pCB->show ( level - 1u );
    }
}
