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
 */

#ifndef epicsAtomic_h
#define epicsAtomic_h

#include <stdlib.h> /* define size_t */

#include "compilerSpecific.h"

#define EPICS_ATOMIC_INLINE static EPICS_ALWAYS_INLINE

#ifdef __cplusplus
extern "C" {
#endif

typedef void * EpicsAtomicPtrT;

/* load target into cache */
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void);

/* push cache version of target into target */
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void);

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, add one to target, flush cache 
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget );
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, subtract one from target, flush cache 
 * to target, allow out other smp processors to access the target,
 * return new value of target as modified by this operation
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget );
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, add/sub delta to/from target, flush cache 
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta );
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta );
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta );

/*
 * set cache version of target, flush cache to target 
 */
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeT  ( size_t * pTarget, size_t newValue );
EPICS_ATOMIC_INLINE void epicsAtomicSetIntT ( int * pTarget, int newValue );
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newValue );

/*
 * fetch target into cache, return new value of target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget );
EPICS_ATOMIC_INLINE int epicsAtomicGetIntT ( const int * pTarget );
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, if target is equal to oldVal set target 
 * to newVal, flush cache to target, allow other smp processors 
 * to access the target, return the original value stored in the
 * target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget,
                                            size_t oldVal, size_t newVal );
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget,
                                            int oldVal, int newVal );
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT (
                                            EpicsAtomicPtrT * pTarget, 
                                            EpicsAtomicPtrT oldVal, 
                                            EpicsAtomicPtrT newVal );

#ifdef __cplusplus
} /* end of extern "C" */
#endif

/*
 * options for in-line compiler intrinsic or OS specific 
 * implementations of the above function prototypes
 *
 * for some of the compilers we must define the
 * in-line functions before they get used in the c++ 
 * in-line functions below
 */
#include "epicsAtomicCD.h"

#ifdef __cplusplus

namespace epics {
namespace atomic {

/* 
 * overloaded c++ interface 
 */

/************* incr ***************/
EPICS_ATOMIC_INLINE size_t increment ( size_t & v )
{
    return epicsAtomicIncrSizeT ( & v );
}

EPICS_ATOMIC_INLINE int increment ( int & v )
{
    return epicsAtomicIncrIntT ( & v );
}

/************* decr ***************/
EPICS_ATOMIC_INLINE size_t decrement ( size_t & v )
{
    return epicsAtomicDecrSizeT ( & v );
}

EPICS_ATOMIC_INLINE int decrement ( int & v )
{
    return epicsAtomicDecrIntT ( & v );
}

/************* add ***************/
EPICS_ATOMIC_INLINE size_t add ( size_t & v, size_t delta )
{
    return epicsAtomicAddSizeT ( & v, delta );
}

EPICS_ATOMIC_INLINE int add ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, delta );
}

/************* sub ***************/
EPICS_ATOMIC_INLINE size_t subtract ( size_t & v, size_t delta )
{
    return epicsAtomicSubSizeT ( & v, delta );
}

EPICS_ATOMIC_INLINE int subtract ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, -delta );
}

/************* set ***************/
EPICS_ATOMIC_INLINE void set ( size_t & v , size_t newValue )
{ 
    epicsAtomicSetSizeT  ( & v, newValue ); 
}

EPICS_ATOMIC_INLINE void set ( int & v, int newValue )
{
    epicsAtomicSetIntT ( & v, newValue );
}

EPICS_ATOMIC_INLINE void set ( EpicsAtomicPtrT & v, EpicsAtomicPtrT newValue )
{
    epicsAtomicSetPtrT ( & v, newValue );
}

/************* get ***************/
EPICS_ATOMIC_INLINE size_t get ( const size_t & v )
{
    return epicsAtomicGetSizeT ( & v );
}

EPICS_ATOMIC_INLINE int get ( const int & v )
{
    return epicsAtomicGetIntT ( & v );
}

EPICS_ATOMIC_INLINE EpicsAtomicPtrT get ( const EpicsAtomicPtrT & v )
{
    return epicsAtomicGetPtrT ( & v );
}

/************* cas ***************/
EPICS_ATOMIC_INLINE size_t compareAndSwap ( size_t & v,
                                          size_t oldVal, size_t newVal )
{
    return epicsAtomicCmpAndSwapSizeT ( & v, oldVal, newVal );
}

EPICS_ATOMIC_INLINE int compareAndSwap ( int & v, int oldVal, int newVal )
{
    return epicsAtomicCmpAndSwapIntT ( & v, oldVal, newVal );
}

EPICS_ATOMIC_INLINE EpicsAtomicPtrT compareAndSwap ( EpicsAtomicPtrT & v,
                                                   EpicsAtomicPtrT oldVal,
                                                   EpicsAtomicPtrT newVal )
{
    return epicsAtomicCmpAndSwapPtrT ( & v, oldVal, newVal );
}

} /* end of name space atomic */
} /* end of name space epics */ 

#endif /* ifdef __cplusplus */

#endif /* epicsAtomic_h */
