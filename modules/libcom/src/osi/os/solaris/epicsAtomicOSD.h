
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

#include "shareLib.h"

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

#define EPICS_ATOMIC_OS_NAME "Solaris"

#if defined ( __SunOS_5_10 )

/* 
 * atomic.h exists only in Solaris 10 or higher
 */
#include <atomic.h>

#include "epicsAssert.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
#define EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    membar_consumer ();
}
#endif

#ifndef EPICS_ATOMIC_WRITE_MEMORY_BARRIER
#define EPICS_ATOMIC_WRITE_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    membar_producer ();
}
#endif

#ifndef EPICS_ATOMIC_CAS_INTT
#define EPICS_ATOMIC_CAS_INTT
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget,
                                               int oldVal, int newVal )
{
    STATIC_ASSERT ( sizeof ( int ) == sizeof ( unsigned ) );
    unsigned * const pTarg = ( unsigned * ) pTarget;
    return ( int ) atomic_cas_uint ( pTarg, ( unsigned ) oldVal, 
                                        ( unsigned ) newVal );
}
#endif

#ifndef EPICS_ATOMIC_CAS_SIZET
#define EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT (
                                                  size_t * pTarget,
                                                  size_t oldVal, size_t newVal )
{
    STATIC_ASSERT ( sizeof ( ulong_t ) == sizeof ( size_t ) );
    ulong_t * const pTarg = ( ulong_t * ) pTarget;
    return ( size_t ) atomic_cas_ulong ( pTarg, oldVal, newVal );
}
#endif

#ifndef EPICS_ATOMIC_CAS_PTRT
#define EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT (
                                       EpicsAtomicPtrT * pTarget, 
                                       EpicsAtomicPtrT oldVal, 
                                       EpicsAtomicPtrT newVal )
{
    return atomic_cas_ptr ( pTarget, oldVal, newVal );
}
#endif

#ifndef EPICS_ATOMIC_INCR_INTT
#define EPICS_ATOMIC_INCR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget )
{
    STATIC_ASSERT ( sizeof ( unsigned ) == sizeof ( int ) );
    unsigned * const pTarg = ( unsigned * ) ( pTarget );
    return ( int ) atomic_inc_uint_nv ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_INCR_SIZET
#define EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    STATIC_ASSERT ( sizeof ( ulong_t  ) == sizeof ( size_t ) );
    ulong_t * const pTarg = ( ulong_t * ) pTarget;
    return ( size_t ) atomic_inc_ulong_nv ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_DECR_INTT
#define EPICS_ATOMIC_DECR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget )
{
    STATIC_ASSERT ( sizeof ( unsigned ) == sizeof ( int ) );
    unsigned * const pTarg = ( unsigned * ) ( pTarget );
    return ( int ) atomic_dec_uint_nv ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_DECR_SIZET
#define EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    STATIC_ASSERT ( sizeof ( ulong_t  ) == sizeof ( size_t ) );
    ulong_t * const pTarg = ( ulong_t * ) pTarget;
    return ( size_t ) atomic_dec_ulong_nv ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_ADD_INTT
#define EPICS_ATOMIC_ADD_INTT
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta )
{
    STATIC_ASSERT ( sizeof ( unsigned ) == sizeof ( int ) );
    unsigned * const pTarg = ( unsigned * ) ( pTarget );
    return ( int ) atomic_add_int_nv ( pTarg, delta );
}
#endif

#ifndef EPICS_ATOMIC_ADD_SIZET
#define EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget,
                                                 size_t delta )
{
    STATIC_ASSERT ( sizeof ( ulong_t  ) == sizeof ( size_t ) );
    ulong_t * const pTarg = ( ulong_t * ) pTarget;
    return ( size_t ) atomic_add_long_nv ( pTarg, ( long ) delta );
}
#endif

#ifndef EPICS_ATOMIC_SUB_SIZET
#define EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget,
                                                 size_t delta )
{
    STATIC_ASSERT ( sizeof ( ulong_t ) == sizeof ( size_t ) );
    ulong_t * const pTarg = ( ulong_t * ) pTarget;
    long sdelta = ( long ) delta;
    return ( size_t ) atomic_add_long_nv ( pTarg, -sdelta );
}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* ifdef __SunOS_5_10 */

typedef struct EpicsAtomicLockKey {
    char dummy;
} EpicsAtomicLockKey;


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

epicsShareFunc void epicsAtomicLock ( struct EpicsAtomicLockKey * );
epicsShareFunc void epicsAtomicUnlock ( struct EpicsAtomicLockKey * );

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#include "epicsAtomicDefault.h"

#endif /* epicsAtomicOSD_h */

