
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
#include "epicsMutex.h"

namespace {
    
class AtomicGuard {
public:
    AtomicGuard ();
    ~AtomicGuard ();
private:
    static epicsMutexOSD & m_mutex;
};

//
// see c++ FAQ, static init order fiasco 
//
epicsMutexOSD & AtomicGuard :: m_mutex = * epicsMutexOsdCreate ();

inline AtomicGuard :: AtomicGuard ()
{
    const int status = epicsMutexOsdLock ( & m_mutex );
    assert ( status == epicsMutexLockOK );
}

inline AtomicGuard :: ~AtomicGuard ()
{
    epicsMutexOsdUnlock ( & m_mutex );
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

size_t epicsLockedGetSizeT ( const size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

unsigned epicsLockedTestAndSetUIntT ( unsigned * pTarget )
{
    AtomicGuard atomicGuard;
    const bool weWillSetIt = ( *pTarget == 0u );
    if ( weWillSetIt ) {
        *pTarget = 1u;
    }
    return weWillSetIt;
}

} // end of extern "C" 




