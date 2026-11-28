/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsAtomic.h
 *
 * \brief OS independent interface to perform atomic operations
 *
 * This is an operating system and compiler independent interface to an operating system and or compiler
 * dependent implementation of several atomic primitives.
 *
 * These primitives can be safely used in a multithreaded programs on symmetric multiprocessing (SMP)
 * systems. Where possible the primitives are implemented with compiler intrinsic wrappers for architecture
 * speciﬁc instructions. Otherwise they are implemented with OS speciﬁc functions and otherwise, when lacking
 *  a suﬃcently capable OS speciﬁc interface, then in some rare situations a mutual exclusion primitive is
 * used for synchronization.
 *
 * In operating systems environments which allow C code to run at interrupt level the implementation must
 * use interrupt level invocable CPU instruction primitives.
 *
 * All C++ functions are implemented in the namespace atomics which is nested inside of namespace epics.
 */
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

/** Argument type for atomic operations on pointers*/
typedef void * EpicsAtomicPtrT;

/** \brief load target into cache
 *
 * load target into cache
 **/
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void);

/** \brief push cache version of target into target
 *
 * push cache version of target into target
 */
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void);

/** \brief atomic increment on size_t value
 *
 * Lock out other smp processors from accessing the target,
 * load target into cache, add one to target, flush cache
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * \param pTarget pointer to target
 *
 * \return New value of target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget );

 /** \brief atomic increment on int value
  *
  * Lock out other smp processors from accessing the target,
  * load target into cache, add one to target, flush cache
  * to target, allow other smp processors to access the target,
  * return new value of target as modified by this operation
  *
  * \param pTarget pointer to target
  *
  * \return New value of target
  */
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget );

/** \brief atomic decrement on size_t value
 *
 * Lock out other smp processors from accessing the target,
 * load target into cache, subtract one from target, flush cache
 * to target, allow out other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * \param pTarget pointer to target
 *
 * \return New value of target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget );

/** \brief atomic decrement on int value
 *
 * Lock out other smp processors from accessing the target,
 * load target into cache, subtract one from target, flush cache
 * to target, allow out other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * \param pTarget pointer to target
 *
 * \return New value of target
 */
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget );

 /** \brief atomic addition on size_t value
  *
  * Lock out other smp processors from accessing the target,
  * load target into cache, add \p delta to target, flush cache
  * to target, allow other smp processors to access the target,
  * return new value of target as modified by this operation
  *
  * \param pTarget pointer to target
  * \param delta value to add to target
  *
  * \return New value of target
  */
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta );

/** \brief atomic subtraction on size_t value
 *
 * Lock out other smp processors from accessing the target,
 * load target into cache, subtract \p delta from target, flush cache
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * \param pTarget pointer to target
 * \param delta value to subtract from target
 *
 * \return New value of target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta );

/** \brief atomic addition on int value
 *
 * Lock out other smp processors from accessing the target,
 * load target into cache, add \p delta to target, flush cache
 * to target, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * \param pTarget pointer to target
 * \param delta value to add to target
 *
 * \return New value of target
 */
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta );

 /** \brief atomically assign size_t value to variable
  *
  * set cache version of target to new value, flush cache to target
  *
  * \param pTarget pointer to target
  * \param newValue desired value of target
  */
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeT  ( size_t * pTarget, size_t newValue );

 /** \brief atomically assign int value to variable
  *
  * set cache version of target to new value, flush cache to target
  *
  * \param pTarget pointer to target
  * \param newValue desired value of target
  */
EPICS_ATOMIC_INLINE void epicsAtomicSetIntT ( int * pTarget, int newValue );

 /** \brief atomically assign pointer value to variable
  *
  * set cache version of target to new value, flush cache to target
  *
  * \param pTarget pointer to target
  * \param newValue desired value of target
  */
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newValue );

/** \brief atomically load and return size_t value
 *
 * fetch target into cache, return new value of target
 *
 * \param pTarget pointer to target
 *
 * \return value of target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget );

/** \brief atomically load and return int value
 *
 * fetch target into cache, return new value of target
 *
 * \param pTarget pointer to target
 *
 * \return value of target
 */
EPICS_ATOMIC_INLINE int epicsAtomicGetIntT ( const int * pTarget );

/** \brief atomically load and return pointer value
 *
 * fetch target into cache, return new value of target
 *
 * \param pTarget pointer to target
 *
 * \return value of target
 */
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget );

/** \brief atomically compare size_t value with expected and if equal swap with new value
 *
 * lock out other smp processors from accessing the target,
 * load target into cache. If target is equal to \p oldVal, set target
 * to \p newVal, flush cache to target, allow other smp processors
 * to access the target
 *
 * \param pTarget pointer to target
 * \param oldVal value that will be compared with target
 * \param newVal value that will be set to target if oldVal == target
 *
 * \return the original value stored in the target
 */
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget,
                                            size_t oldVal, size_t newVal );

/** \brief atomically compare int value with expected and if equal swap with new value
 *
 * lock out other smp processors from accessing the target,
 * load target into cache. If target is equal to \p oldVal, set target
 * to \p newVal, flush cache to target, allow other smp processors
 * to access the target
 *
 * \param pTarget pointer to target
 * \param oldVal value that will be compared with target
 * \param newVal value that will be set to target if oldVal == target
 *
 * \return the original value stored in the target
 */
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget,
                                            int oldVal, int newVal );

/** \brief atomically compare int value with expected and if equal swap with new value
 *
 * lock out other smp processors from accessing the target,
 * load target into cache. If target is equal to \p oldVal, set target
 * to \p newVal, flush cache to target, allow other smp processors
 * to access the target
 *
 * \param pTarget pointer to target
 * \param oldVal value that will be compared with target
 * \param newVal value that will be set to target if oldVal == target
 *
 * \return the original value stored in the target
 */
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT (
                                            EpicsAtomicPtrT * pTarget,
                                            EpicsAtomicPtrT oldVal,
                                            EpicsAtomicPtrT newVal );

/** \name Atomic exchange
 *
 * Atomically replace the target with \p newVal and return the value
 * it held before. Like the other read-modify-write operations above,
 * an exchange is a full memory barrier.
 *
 * \param pTarget pointer to target
 * \param newVal value to store in target
 *
 * \return the original value stored in the target
 *
 * \since UNRELEASED
 */
/** @{ */
EPICS_ATOMIC_INLINE int epicsAtomicExchangeIntT ( int * pTarget, int newVal );
EPICS_ATOMIC_INLINE size_t epicsAtomicExchangeSizeT ( size_t * pTarget,
                                            size_t newVal );
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicExchangePtrT (
                                            EpicsAtomicPtrT * pTarget,
                                            EpicsAtomicPtrT newVal );
/** @} */

/** \name Loads and stores with explicit memory ordering
 *
 * These functions load or store a target atomically, i.e. without
 * tearing, and with a memory ordering as defined by C11 and C++11:
 *
 * - \b Relaxed: the access is atomic, but does not order any other
 *   memory accesses of the calling thread. Use for counters, flags
 *   and statistics that do not protect other data.
 * - \b Release (stores only): memory accesses of the calling thread
 *   that come before the store in program order cannot be moved after
 *   it.
 * - \b Acquire (loads only): memory accesses of the calling thread
 *   that come after the load in program order cannot be moved before
 *   it.
 *
 * If a thread reads with an acquire load the value written by
 * a release store of another thread, all memory writes the other thread
 * made before its release store are visible to the reading thread after
 * its acquire load. This is the usual pattern to publish data through
 * a flag or a pointer:
 *
 * \code
 * // producer                         // consumer
 * pNode->value = 42;                  node *p = epicsAtomicGetPtrTAcquire(&pHead);
 * epicsAtomicSetPtrTRelease(&pHead,   if (p)
 *                           pNode);       assert(p->value == 42);
 * \endcode
 *
 * Unlike epicsAtomicGetIntT() and friends, which combine a plain access
 * with a full memory barrier, these functions use the cheapest
 * instructions the target provides: on x86 and x86_64 they all compile
 * to plain moves, on ARM and POWER acquire and release need only
 * lightweight barriers, and relaxed accesses need none.
 *
 * Where the compiler provides no suitable intrinsics, they are
 * implemented with volatile accesses and (except for the relaxed
 * variants) both epicsAtomicReadMemoryBarrier() and
 * epicsAtomicWriteMemoryBarrier().
 *
 * \param pTarget pointer to target
 * \param newVal value to store in target
 *
 * \return value of target (loads)
 *
 * \since UNRELEASED
 */
/** @{ */
EPICS_ATOMIC_INLINE int epicsAtomicGetIntTRelaxed ( const int * pTarget );
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeTRelaxed ( const size_t * pTarget );
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrTRelaxed (
                                            const EpicsAtomicPtrT * pTarget );

EPICS_ATOMIC_INLINE int epicsAtomicGetIntTAcquire ( const int * pTarget );
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeTAcquire ( const size_t * pTarget );
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrTAcquire (
                                            const EpicsAtomicPtrT * pTarget );

EPICS_ATOMIC_INLINE void epicsAtomicSetIntTRelaxed ( int * pTarget, int newVal );
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeTRelaxed ( size_t * pTarget,
                                            size_t newVal );
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrTRelaxed ( EpicsAtomicPtrT * pTarget,
                                            EpicsAtomicPtrT newVal );

EPICS_ATOMIC_INLINE void epicsAtomicSetIntTRelease ( int * pTarget, int newVal );
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeTRelease ( size_t * pTarget,
                                            size_t newVal );
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrTRelease ( EpicsAtomicPtrT * pTarget,
                                            EpicsAtomicPtrT newVal );
/** @} */

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

/** \brief C++ API for atomic size_t increment
 *
 * C++ API for atomic size_t increment.
 *
 * \param v variable to increment
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE size_t increment ( size_t & v )
{
    return epicsAtomicIncrSizeT ( & v );
}

/** \brief C++ API for atomic int increment
 *
 * C++ API for atomic int increment.
 *
 * \param v variable to increment
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE int increment ( int & v )
{
    return epicsAtomicIncrIntT ( & v );
}

/** \brief C++ API for atomic size_t decrement
 *
 * C++ API for atomic size_t decrement
 *
 * \param v variable to decrement
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE size_t decrement ( size_t & v )
{
    return epicsAtomicDecrSizeT ( & v );
}

/** \brief C++ API for atomic int decrement
 *
 * C++ API for atomic int decrement
 *
 * \param v variable to decrement
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE int decrement ( int & v )
{
    return epicsAtomicDecrIntT ( & v );
}

/** \brief C++ API for atomic size_t addition
 *
 * C++ API for atomic size_t addition
 *
 * \param v variable to add to
 * \param delta value to add to \p v
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE size_t add ( size_t & v, size_t delta )
{
    return epicsAtomicAddSizeT ( & v, delta );
}

/** \brief C++ API for atomic int addition
 *
 * C++ API for atomic int addition
 *
 * \param v variable to add to
 * \param delta value to add to \p v
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE int add ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, delta );
}

/** \brief C++ API for atomic size_t subtraction
 *
 * C++ API for atomic size_t subtraction
 *
 * \param v variable to subtract from
 * \param delta value to subtract from \p v
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE size_t subtract ( size_t & v, size_t delta )
{
    return epicsAtomicSubSizeT ( & v, delta );
}

/** \brief C++ API for atomic int subtraction
 *
 * C++ API for atomic int subtraction
 *
 * \param v variable to subtract from
 * \param delta value to subtract from \p v
 *
 * \return new value
 */
EPICS_ATOMIC_INLINE int subtract ( int & v, int delta )
{
    return epicsAtomicAddIntT ( & v, -delta );
}

/** \brief C++ API for atomic size_t assignment
 *
 * C++ API for atomic size_t assignment
 *
 * \param v variable to assign to
 * \param newValue new value for \p v
 */
EPICS_ATOMIC_INLINE void set ( size_t & v , size_t newValue )
{
    epicsAtomicSetSizeT  ( & v, newValue );
}

/** \brief C++ API for atomic int assignment
 *
 * C++ API for atomic int assignment
 *
 * \param v variable to assign to
 * \param newValue new value for \p v
 */
EPICS_ATOMIC_INLINE void set ( int & v, int newValue )
{
    epicsAtomicSetIntT ( & v, newValue );
}

/** \brief C++ API for atomic pointer assignment
 *
 * C++ API for atomic pointer assignment
 *
 * \param v variable to assign to
 * \param newValue new value for \p v
 */
EPICS_ATOMIC_INLINE void set ( EpicsAtomicPtrT & v, EpicsAtomicPtrT newValue )
{
    epicsAtomicSetPtrT ( & v, newValue );
}

/** \brief C++ API for atomic size_t load value
 *
 * C++ API for atomic size_t load value
 *
 * \param v variable to load
 *
 * \return value of \p v
 */
EPICS_ATOMIC_INLINE size_t get ( const size_t & v )
{
    return epicsAtomicGetSizeT ( & v );
}

/** \brief C++ API for atomic int load value
 *
 * C++ API for atomic int load value
 *
 * \param v variable to load
 *
 * \return value of \p v
 */
EPICS_ATOMIC_INLINE int get ( const int & v )
{
    return epicsAtomicGetIntT ( & v );
}

/** \brief C++ API for atomic pointer load value
 *
 * C++ API for atomic pointer load value
 *
 * \param v variable to load
 *
 * \return value of \p v
 */
EPICS_ATOMIC_INLINE EpicsAtomicPtrT get ( const EpicsAtomicPtrT & v )
{
    return epicsAtomicGetPtrT ( & v );
}

/** \brief C++ API for atomic size_t compare-and-swap
 *
 * C++ API for atomic size_t compare-and-swap. Atomic operation that compares \p v with \p oldVal
 * and if \p v == \v oldVal, sets \p v to \v newVal
 *
 * \param v variable to compare and swap
 * \param oldVal value to compare to \p \v
 * \param newVal value to set to \p v
 *
 * \return original value stored in \p v
 */
EPICS_ATOMIC_INLINE size_t compareAndSwap ( size_t & v,
                                          size_t oldVal, size_t newVal )
{
    return epicsAtomicCmpAndSwapSizeT ( & v, oldVal, newVal );
}

/** \brief C++ API for atomic int compare-and-swap
 *
 * C++ API for atomic size_t compare-and-swap. Atomic operation that compares \p v with \p oldVal
 * and if \p v == \v oldVal, sets \p v to \v newVal
 *
 * \param v variable to compare and swap
 * \param oldVal value to compare to \p \v
 * \param newVal value to set to \p v
 *
 * \return original value stored in \p v
 */
EPICS_ATOMIC_INLINE int compareAndSwap ( int & v, int oldVal, int newVal )
{
    return epicsAtomicCmpAndSwapIntT ( & v, oldVal, newVal );
}

/** \brief C++ API for atomic pointer compare-and-swap
 *
 * C++ API for atomic size_t compare-and-swap. Atomic operation that compares \p v with \p oldVal
 * and if \p v == \v oldVal, sets \p v to \v newVal
 *
 * \param v variable to compare and swap
 * \param oldVal value to compare to \p \v
 * \param newVal value to set to \p v
 *
 * \return original value stored in \p v
 */
EPICS_ATOMIC_INLINE EpicsAtomicPtrT compareAndSwap ( EpicsAtomicPtrT & v,
                                                   EpicsAtomicPtrT oldVal,
                                                   EpicsAtomicPtrT newVal )
{
    return epicsAtomicCmpAndSwapPtrT ( & v, oldVal, newVal );
}

/** \name C++ API for atomic exchange
 *
 * See epicsAtomicExchangeIntT()
 *
 * \since UNRELEASED
 */
/** @{ */
EPICS_ATOMIC_INLINE int exchange ( int & v, int newVal )
{
    return epicsAtomicExchangeIntT ( & v, newVal );
}

EPICS_ATOMIC_INLINE size_t exchange ( size_t & v, size_t newVal )
{
    return epicsAtomicExchangeSizeT ( & v, newVal );
}

EPICS_ATOMIC_INLINE EpicsAtomicPtrT exchange ( EpicsAtomicPtrT & v,
                                               EpicsAtomicPtrT newVal )
{
    return epicsAtomicExchangePtrT ( & v, newVal );
}
/** @} */

/** \name C++ API for loads and stores with explicit memory ordering
 *
 * See epicsAtomicGetIntTRelaxed()
 *
 * \since UNRELEASED
 */
/** @{ */
EPICS_ATOMIC_INLINE int getRelaxed ( const int & v )
{
    return epicsAtomicGetIntTRelaxed ( & v );
}

EPICS_ATOMIC_INLINE size_t getRelaxed ( const size_t & v )
{
    return epicsAtomicGetSizeTRelaxed ( & v );
}

EPICS_ATOMIC_INLINE EpicsAtomicPtrT getRelaxed ( const EpicsAtomicPtrT & v )
{
    return epicsAtomicGetPtrTRelaxed ( & v );
}

EPICS_ATOMIC_INLINE int getAcquire ( const int & v )
{
    return epicsAtomicGetIntTAcquire ( & v );
}

EPICS_ATOMIC_INLINE size_t getAcquire ( const size_t & v )
{
    return epicsAtomicGetSizeTAcquire ( & v );
}

EPICS_ATOMIC_INLINE EpicsAtomicPtrT getAcquire ( const EpicsAtomicPtrT & v )
{
    return epicsAtomicGetPtrTAcquire ( & v );
}

EPICS_ATOMIC_INLINE void setRelaxed ( int & v, int newVal )
{
    epicsAtomicSetIntTRelaxed ( & v, newVal );
}

EPICS_ATOMIC_INLINE void setRelaxed ( size_t & v, size_t newVal )
{
    epicsAtomicSetSizeTRelaxed ( & v, newVal );
}

EPICS_ATOMIC_INLINE void setRelaxed ( EpicsAtomicPtrT & v,
                                      EpicsAtomicPtrT newVal )
{
    epicsAtomicSetPtrTRelaxed ( & v, newVal );
}

EPICS_ATOMIC_INLINE void setRelease ( int & v, int newVal )
{
    epicsAtomicSetIntTRelease ( & v, newVal );
}

EPICS_ATOMIC_INLINE void setRelease ( size_t & v, size_t newVal )
{
    epicsAtomicSetSizeTRelease ( & v, newVal );
}

EPICS_ATOMIC_INLINE void setRelease ( EpicsAtomicPtrT & v,
                                      EpicsAtomicPtrT newVal )
{
    epicsAtomicSetPtrTRelease ( & v, newVal );
}
/** @} */

} /* end of name space atomic */
} /* end of name space epics */

#endif /* ifdef __cplusplus */

#endif /* epicsAtomic_h */
