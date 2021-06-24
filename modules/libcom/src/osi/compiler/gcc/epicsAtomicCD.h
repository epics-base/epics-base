
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicCD_h
#define epicsAtomicCD_h

#ifndef __GNUC__
#   error this header is only for use with the gnu compiler
#endif

#define EPICS_ATOMIC_CMPLR_NAME "GCC"

/* expands __GCC_HAVE_SYNC_COMPARE_AND_SWAP_ concatentating the numeric value __SIZEOF_*__ */
#define GCC_ATOMIC_CONCAT( A, B ) GCC_ATOMIC_CONCATR(A,B)
#define GCC_ATOMIC_CONCATR( A, B ) ( A ## B )

/*
 * As of GCC 8, the __sync_synchronize() is inlined for all
 * known targets (aarch64, arm, i386, powerpc, and x86_64)
 * except for arm <=6.
 * Note that i386 inlines __sync_synchronize() but does not
 * define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_*
 */
#define GCC_ATOMIC_INTRINSICS_AVAIL_SYNC \
    defined(GCC_ATOMIC_INTRINSICS_AVAIL_INT_T) || defined(GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T) || defined(__i386)

#define GCC_ATOMIC_INTRINSICS_AVAIL_INT_T \
    GCC_ATOMIC_CONCAT ( \
        __GCC_HAVE_SYNC_COMPARE_AND_SWAP_, \
        __SIZEOF_INT__ )

/* we assume __SIZEOF_POINTER__ == __SIZEOF_SIZE_T__ */
#define GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T \
    GCC_ATOMIC_CONCAT ( \
        __GCC_HAVE_SYNC_COMPARE_AND_SWAP_, \
        __SIZEOF_SIZE_T__ )

#ifdef __cplusplus
extern "C" {
#endif

#if GCC_ATOMIC_INTRINSICS_AVAIL_SYNC

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
#define EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    __sync_synchronize ();
}
#endif

#ifndef EPICS_ATOMIC_WRITE_MEMORY_BARRIER
#define EPICS_ATOMIC_WRITE_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    __sync_synchronize ();
}
#endif

#endif

#if GCC_ATOMIC_INTRINSICS_AVAIL_INT_T

#define EPICS_ATOMIC_INCR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget )
{
    return __sync_add_and_fetch ( pTarget, 1 );
}

#define EPICS_ATOMIC_DECR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget )
{
    return __sync_sub_and_fetch ( pTarget, 1 );
}

#define EPICS_ATOMIC_ADD_INTT
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta )
{
    return __sync_add_and_fetch ( pTarget, delta );
}

#define EPICS_ATOMIC_CAS_INTT
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget,
                                        int oldVal, int newVal )
{
    return __sync_val_compare_and_swap ( pTarget, oldVal, newVal);
}

#endif

#if GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T

#define EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    return __sync_add_and_fetch ( pTarget, 1u );
}

#define EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    return __sync_sub_and_fetch ( pTarget, 1u );
}

#define EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta )
{
    return __sync_add_and_fetch ( pTarget, delta );
}

#define EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta )
{
    return __sync_sub_and_fetch ( pTarget, delta );
}

#define EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget,
                                        size_t oldVal, size_t newVal )
{
    return __sync_val_compare_and_swap ( pTarget, oldVal, newVal);
}

#define EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT (
                            EpicsAtomicPtrT * pTarget,
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    return __sync_val_compare_and_swap ( pTarget, oldVal, newVal);
}

#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif

/*
 * if currently unavailable as gcc intrinsics we
 * will try for an os specific inline solution
 */
#include "epicsAtomicOSD.h"

#endif /* epicsAtomicCD_h */
