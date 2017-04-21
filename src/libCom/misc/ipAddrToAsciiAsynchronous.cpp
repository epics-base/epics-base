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
#include <cstdio>

//#define EPICS_FREELIST_DEBUG
#define EPICS_PRIVATE_API

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
    osiSockAddr addr;
    ipAddrToAsciiEnginePrivate & engine;
    ipAddrToAsciiCallBack * pCB;
    bool pending;
    void ipAddrToAscii ( const osiSockAddr &, ipAddrToAsciiCallBack & );
    void release (); 
    void operator delete ( void * );
private:
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

namespace {
struct ipAddrToAsciiGlobal : public epicsThreadRunable {
    ipAddrToAsciiGlobal();
    virtual ~ipAddrToAsciiGlobal() {}

    virtual void run ();

    char nameTmp [1024];
    tsFreeList
        < ipAddrToAsciiTransactionPrivate, 0x80 >
            transactionFreeList;
    tsDLList < ipAddrToAsciiTransactionPrivate > labor;
    mutable epicsMutex mutex;
    epicsEvent laborEvent;
    epicsEvent destructorBlockEvent;
    epicsThread thread;
    // pCurrent may be changed by any thread (worker or other)
    ipAddrToAsciiTransactionPrivate * pCurrent;
    // pActive may only be changed by the worker
    ipAddrToAsciiTransactionPrivate * pActive;
    unsigned cancelPendingCount;
    bool exitFlag;
    bool callbackInProgress;
};
}

// - this class executes the synchronous DNS query
// - it creates one thread
class ipAddrToAsciiEnginePrivate : 
    public ipAddrToAsciiEngine {
public:
    ipAddrToAsciiEnginePrivate() :refcount(1u), released(false) {}
    virtual ~ipAddrToAsciiEnginePrivate () {}
    void show ( unsigned level ) const; 

    unsigned refcount;
    bool released;

    static ipAddrToAsciiGlobal * pEngine;
    ipAddrToAsciiTransaction & createTransaction ();
    void release ();

private:
    ipAddrToAsciiEnginePrivate ( const ipAddrToAsciiEngine & );
	ipAddrToAsciiEnginePrivate & operator = ( const ipAddrToAsciiEngine & );
};

ipAddrToAsciiGlobal * ipAddrToAsciiEnginePrivate :: pEngine = 0;
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
    try {
        ipAddrToAsciiEnginePrivate::pEngine = new ipAddrToAsciiGlobal ();
    } catch (std::exception& e) {
        errlogPrintf("ipAddrToAsciiEnginePrivate ctor fails with: %s\n", e.what());
    }
}

void ipAddrToAsciiEngine::cleanup()
{
    {
        epicsGuard<epicsMutex> G(ipAddrToAsciiEnginePrivate::pEngine->mutex);
        ipAddrToAsciiEnginePrivate::pEngine->exitFlag = true;
    }
    ipAddrToAsciiEnginePrivate::pEngine->laborEvent.signal();
    ipAddrToAsciiEnginePrivate::pEngine->thread.exitWait();
    delete ipAddrToAsciiEnginePrivate::pEngine;
    ipAddrToAsciiEnginePrivate::pEngine = 0;
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
    return * new ipAddrToAsciiEnginePrivate();
}

ipAddrToAsciiGlobal::ipAddrToAsciiGlobal () :
    thread ( *this, "ipToAsciiProxy",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow ),
    pCurrent ( 0 ), pActive ( 0 ), cancelPendingCount ( 0u ), exitFlag ( false ),
    callbackInProgress ( false )
{
    this->thread.start (); // start the thread
}


void ipAddrToAsciiEnginePrivate::release ()
{
    bool last;
    {
        epicsGuard < epicsMutex > guard ( this->pEngine->mutex );
        if(released)
            throw std::logic_error("Engine release() called again!");

        // released==true prevents new transactions
        released = true;

        {
            // cancel any pending transactions
            tsDLIter < ipAddrToAsciiTransactionPrivate > it(pEngine->labor.firstIter());
            while(it.valid()) {
                ipAddrToAsciiTransactionPrivate *trn = it.pointer();
                ++it;

                if(this==&trn->engine) {
                    trn->pending = false;
                    pEngine->labor.remove(*trn);
                }
            }

            // cancel transaction in lookup or callback
            if (pEngine->pCurrent && this==&pEngine->pCurrent->engine) {
                pEngine->pCurrent->pending = false;
                pEngine->pCurrent = 0;
            }

            // wait for completion of in-progress callback
            pEngine->cancelPendingCount++;
            while(pEngine->pActive && this==&pEngine->pActive->engine
                  && ! pEngine->thread.isCurrentThread()) {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                pEngine->destructorBlockEvent.wait();
            }
            pEngine->cancelPendingCount--;
            if(pEngine->cancelPendingCount)
                pEngine->destructorBlockEvent.signal();
        }

        assert(refcount>0);
        last = 0==--refcount;
    }
    if(last) {
        delete this;
    }
}

void ipAddrToAsciiEnginePrivate::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->pEngine->mutex );
    printf ( "ipAddrToAsciiEngine at %p with %u requests pending\n", 
        static_cast <const void *> (this), this->pEngine->labor.count () );
    if ( level > 0u ) {
        tsDLIter < ipAddrToAsciiTransactionPrivate >
            pItem = this->pEngine->labor.firstIter ();
        while ( pItem.valid () ) {
            pItem->show ( level - 1u );
            pItem++;
        }
    }
    if ( level > 1u ) {
        printf ( "mutex:\n" );
        this->pEngine->mutex.show ( level - 2u );
        printf ( "laborEvent:\n" );
        this->pEngine->laborEvent.show ( level - 2u );
        printf ( "exitFlag  boolean = %u\n", this->pEngine->exitFlag );
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
    epicsGuard <epicsMutex> G(this->pEngine->mutex);
    if(this->released)
        throw std::logic_error("createTransaction() on release()'d ipAddrToAsciiEngine");

    assert(this->refcount>0);

    ipAddrToAsciiTransactionPrivate *ret = new ( this->pEngine->transactionFreeList ) ipAddrToAsciiTransactionPrivate ( *this );

    this->refcount++;

    return * ret;
}

void ipAddrToAsciiGlobal::run ()
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
            ipAddrToAsciiTransactionPrivate *pCur = pActive = pCurrent;
            this->callbackInProgress = true;

            {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                // dont call callback with lock applied
                pCur->pCB->transactionComplete ( this->nameTmp );
            }

            this->callbackInProgress = false;
            pActive = 0;

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
    this->engine.pEngine->transactionFreeList.release ( this );
}

ipAddrToAsciiTransactionPrivate::~ipAddrToAsciiTransactionPrivate ()
{
    ipAddrToAsciiGlobal *pGlobal = this->engine.pEngine;
    bool last;
    {
        epicsGuard < epicsMutex > guard ( pGlobal->mutex );
        while ( this->pending ) {
            if ( pGlobal->pCurrent == this &&
                    pGlobal->callbackInProgress &&
                    ! pGlobal->thread.isCurrentThread() ) {
                // cancel from another thread while callback in progress
                // waits for callback to complete
                assert ( pGlobal->cancelPendingCount < UINT_MAX );
                pGlobal->cancelPendingCount++;
                {
                    epicsGuardRelease < epicsMutex > unguard ( guard );
                    pGlobal->destructorBlockEvent.wait ();
                }
                assert ( pGlobal->cancelPendingCount > 0u );
                pGlobal->cancelPendingCount--;
                if ( ! this->pending ) {
                    if ( pGlobal->cancelPendingCount ) {
                        pGlobal->destructorBlockEvent.signal ();
                    }
                    break;
                }
            }
            else {
                if ( pGlobal->pCurrent == this ) {
                    // cancel from callback, or while lookup in progress
                    pGlobal->pCurrent = 0;
                }
                else {
                    // cancel before lookup starts
                    pGlobal->labor.remove ( *this );
                }
                this->pending = false;
            }
        }
        assert(this->engine.refcount>0);
        last = 0==--this->engine.refcount;
    }
    if(last) {
        delete &this->engine;
    }
}

void ipAddrToAsciiTransactionPrivate::ipAddrToAscii ( 
    const osiSockAddr & addrIn, ipAddrToAsciiCallBack & cbIn )
{
    bool success;
    ipAddrToAsciiGlobal *pGlobal = this->engine.pEngine;

    {
        epicsGuard < epicsMutex > guard ( pGlobal->mutex );

        if (this->engine.released) {
            errlogPrintf("Warning: ipAddrToAscii on transaction with release()'d ipAddrToAsciiEngine");
            success = false;

        } else if ( !this->pending && pGlobal->labor.count () < 16u ) {
            // put some reasonable limit on queue expansion
            this->addr = addrIn;
            this->pCB = & cbIn;
            this->pending = true;
            pGlobal->labor.add ( *this );
            success = true;
        }
        else {
            success = false;
        }
    }

    if ( success ) {
        pGlobal->laborEvent.signal ();
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
    epicsGuard < epicsMutex > guard ( this->engine.pEngine->mutex );
    char ipAddr [64];
    sockAddrToDottedIP ( &this->addr.sa, ipAddr, sizeof ( ipAddr ) );
    printf ( "ipAddrToAsciiTransactionPrivate for address %s\n", ipAddr );
    if ( level > 0u ) {
        printf ( "\tengine %p\n",
            static_cast <void *> ( & this->engine ) );
        this->pCB->show ( level - 1u );
    }
}
