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

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * EpicsAtomicPtrT;

/* load target into cache */
epicsShareFunc void epicsAtomicReadMemoryBarrier ();

/* push cache version of target into target */
epicsShareFunc void epicsAtomicWriteMemoryBarrier ();

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, add one to target, flush cache 
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 */
epicsShareFunc size_t epicsAtomicIncrSizeT ( size_t * pTarget );
epicsShareFunc int epicsAtomicIncrIntT ( int * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, subtract one from target, flush cache 
 * to target, allow out other smp processors to access the target,
 * return new value of target as modified by this operation
 */
epicsShareFunc size_t epicsAtomicDecrSizeT ( size_t * pTarget );
epicsShareFunc int epicsAtomicDecrIntT ( int * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, add/sub delta to/from target, flush cache 
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 */
epicsShareFunc size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta );
epicsShareFunc size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta );
epicsShareFunc int epicsAtomicAddIntT ( int * pTarget, int delta );

/*
 * set cache version of target, flush cache to target 
 */
epicsShareFunc void epicsAtomicSetSizeT  ( size_t * pTarget, size_t newValue ); 
epicsShareFunc void epicsAtomicSetIntT ( int * pTarget, int newValue );
epicsShareFunc void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newValue );

/*
 * fetch target into cache, return new value of target
 */
epicsShareFunc size_t epicsAtomicGetSizeT ( const size_t * pTarget );
epicsShareFunc int epicsAtomicGetIntT ( const int * pTarget );
epicsShareFunc EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * load target into cache, if target is equal to oldVal set target 
 * to newVal, flush cache to target, allow other smp processors 
 * to access the target, return the original value stored in the
 * target
 */
epicsShareFunc size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget, 
                                            size_t oldVal, size_t newVal );
epicsShareFunc int epicsAtomicCmpAndSwapIntT ( int * pTarget, 
                                            int oldVal, int newVal );
epicsShareFunc EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( 
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
epicsShareFunc size_t increment ( size_t & v );
epicsShareFunc int increment ( int & v );
epicsShareFunc size_t decrement ( size_t & v );
epicsShareFunc int decrement ( int & v );
epicsShareFunc size_t add ( size_t & v, size_t delta );
epicsShareFunc int add ( int & v, int delta );
epicsShareFunc size_t subtract ( size_t & v, size_t delta );
epicsShareFunc int subtract ( int & v, int delta );
epicsShareFunc void set ( size_t & v , size_t newValue );
epicsShareFunc void set ( int & v, int newValue );
epicsShareFunc void set ( EpicsAtomicPtrT & v, 
                                     EpicsAtomicPtrT newValue );
epicsShareFunc size_t get ( const size_t & v );
epicsShareFunc int get ( const int & v );
epicsShareFunc EpicsAtomicPtrT get ( const EpicsAtomicPtrT & v );
epicsShareFunc size_t compareAndSwap ( size_t & v, size_t oldVal, 
                                                       size_t newVal );
epicsShareFunc int compareAndSwap ( int & v, int oldVal, int newVal );
epicsShareFunc EpicsAtomicPtrT compareAndSwap ( EpicsAtomicPtrT & v, 
                                                EpicsAtomicPtrT oldVal, 
                                                EpicsAtomicPtrT newVal );

/************* incr ***************/
inline size_t increment ( size_t & v )
{
    return epicsAtomicIncrSizeT ( & v );
}

inline int increment ( int & v )
{
    return epicsAtomicIncrIntT ( & v );
}

/************* decr ***************/
inline size_t decrement ( size_t & v )
{
    return epicsAtomicDecrSizeT ( & v );
}

inline int decrement ( int & v )
{
    return epicsAtomicDecrIntT ( & v );
}

/************* add ***************/
inline size_t add ( size_t & v, size_t delta )
{
    return epicsAtomicAddSizeT ( & v, delta );
}

inline int add ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, delta );
}

/************* sub ***************/
inline size_t subtract ( size_t & v, size_t delta )
{
    return epicsAtomicSubSizeT ( & v, delta );
}

inline int subtract ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, -delta );
}

/************* set ***************/
inline void set ( size_t & v , size_t newValue ) 
{ 
    epicsAtomicSetSizeT  ( & v, newValue ); 
}

inline void set ( int & v, int newValue )
{
    epicsAtomicSetIntT ( & v, newValue );
}

inline void set ( EpicsAtomicPtrT & v, EpicsAtomicPtrT newValue )
{
    epicsAtomicSetPtrT ( & v, newValue );
}

/************* get ***************/
inline size_t get ( const size_t & v )
{
    return epicsAtomicGetSizeT ( & v );
}

inline int get ( const int & v )
{
    return epicsAtomicGetIntT ( & v );
}

inline EpicsAtomicPtrT get ( const EpicsAtomicPtrT & v )
{
    return epicsAtomicGetPtrT ( & v );
}

/************* cas ***************/
inline size_t compareAndSwap ( size_t & v, 
                                           size_t oldVal, size_t newVal )
{
    return epicsAtomicCmpAndSwapSizeT ( & v, oldVal, newVal );
}

inline int compareAndSwap ( int & v, int oldVal, int newVal )
{
    return epicsAtomicCmpAndSwapIntT ( & v, oldVal, newVal );
}

inline EpicsAtomicPtrT compareAndSwap ( EpicsAtomicPtrT & v, 
                                            EpicsAtomicPtrT oldVal, 
                                            EpicsAtomicPtrT newVal )
{
    return epicsAtomicCmpAndSwapPtrT ( & v, oldVal, newVal );
}

} /* end of name space atomic */
} /* end of name space epics */ 

#endif /* ifdef __cplusplus */

#endif /* epicsAtomic_h */
