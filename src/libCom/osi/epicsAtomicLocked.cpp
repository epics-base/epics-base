/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *
 * Provide a global mutex version of the atomic functions for when 
 * we dont have more efficent OS primitives or compiler intriniscs 
 * to use instead.
 *
 * We implement these mutex-based primitives upon the libCom private 
 * interface epicsMutexOsdXxxx because, in libCom, it is convenient 
 * to make this a standalone primitive upon which we can implement 
 * epicsMutex.
 */

#define epicsExportSharedSymbols
#include "epicsAtomic.h"
#include "epicsThread.h"
#include "epicsMutex.h"

namespace {
    
class AtomicGuard {
public:
    AtomicGuard ();
    ~AtomicGuard ();
private:
    static epicsMutexOSD * m_pMutex;
    static epicsThreadOnceId m_onceFlag;
    static void m_once ( void * );
};

//
// c++ 0x specifies the behavior for concurrent
// access to block scope statics but some compiler
// writers, lacking clear guidance in the earlier
// c++ standards, curiously implement thread unsafe 
// block static variables despite ensuring for 
// proper multithreaded behavior for many other
// parst of the compiler infrastructure such as
// runtime support for exception handling
//
// since this is potentially used by the implementation
// of staticInstance we cant use it here and must use
// epicsThreadOnce despite its perfomance pentalty
//
// using epicsThreadOnce here (at this time) increases 
// the overhead of AtomicGuard by as much as 100% 
//
epicsMutexOSD * AtomicGuard :: m_pMutex = 0;
epicsThreadOnceId AtomicGuard :: m_onceFlag = EPICS_THREAD_ONCE_INIT;

void AtomicGuard :: m_once ( void * )
{
    m_pMutex = epicsMutexOsdCreate ();
}

inline AtomicGuard :: AtomicGuard ()
{

    epicsThreadOnce ( & m_onceFlag, m_once, 0 );
    const int status = epicsMutexOsdLock ( m_pMutex );
    assert ( status == epicsMutexLockOK );
}

inline AtomicGuard :: ~AtomicGuard ()
{
    epicsMutexOsdUnlock ( m_pMutex );
}

} // end of anonymous namespace

extern "C" {

size_t epicsLockedIncrSizeT ( size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return ++(*pTarget);
}

size_t epicsLockedDecrSizeT ( size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return --(*pTarget);
}

void epicsLockedSetSizeT ( size_t * pTarget, size_t newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

void epicsLockedSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

void epicsLockedSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

unsigned epicsLockedGetUIntT ( const unsigned * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

size_t epicsLockedGetSizeT ( const size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

EpicsAtomicPtrT epicsLockedGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

unsigned epicsLockedCmpAndSwapUIntT ( unsigned * pTarget, 
                            unsigned oldval, unsigned newval )
{
    AtomicGuard atomicGuard;
    const unsigned cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    return cur;
}

EpicsAtomicPtrT epicsLockedCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldval, EpicsAtomicPtrT newval )
{
    AtomicGuard atomicGuard;
    const EpicsAtomicPtrT cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    return cur;
}

} // end of extern "C" 

