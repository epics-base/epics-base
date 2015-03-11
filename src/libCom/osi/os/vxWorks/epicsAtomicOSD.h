/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne, LLC, as Operator of 
*     Argonne National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#ifndef _VSB_CONFIG_FILE
#   define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>
#endif

#include "vxWorks.h" /* obtain the version of vxWorks */
#include "epicsAssert.h"

/*
 * With vxWorks 6.6 and later we need to use vxAtomicLib
 * to implement this functionality correctly on SMP systems
 */
#if _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606

#include <limits.h>
#include <vxAtomicLib.h>

#define EPICS_ATOMIC_OS_NAME "VX-ATOMICLIB"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
#define EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    VX_MEM_BARRIER_R ();
}
#endif

#ifndef EPICS_ATOMIC_WRITE_MEMORY_BARRIER
#define EPICS_ATOMIC_WRITE_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    VX_MEM_BARRIER_W ();
}
#endif

/*
 * we make the probably correct guess that if ULONG_MAX 
 * is the same as UINT_MAX then sizeof ( atomic_t )
 * will be the same as sizeof ( size_t )
 *
 * if ULONG_MAX != UINT_MAX then its 64 bit vxWorks and
 * WRS doesnt not supply at this time the atomic interface 
 * for 8 byte integers that is needed - so that architecture 
 * receives the lock synchronized version
 */
#if ULONG_MAX == UINT_MAX

STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( size_t ) );
STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( EpicsAtomicPtrT ) );


#ifndef EPICS_ATOMIC_INCR_SIZET
#define EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicInc ( pTarg );
    return 1 + ( size_t ) ( oldVal );
}
#endif

#ifndef EPICS_ATOMIC_DECR_SIZET
#define EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicDec ( pTarg );
    return ( ( size_t ) oldVal ) - 1u;
}
#endif

#ifndef EPICS_ATOMIC_ADD_SIZET
#define EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta )
{
    /* 
     * vxAtomicLib doc indicates that vxAtomicAdd is 
     * implemented using signed arithmetic, but it
     * does not change the end result because twos
     * complement addition is used in either case
     */
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicAdd ( pTarg, (atomic_t) delta );
    return delta + ( size_t ) oldVal;
} 
#endif 

#ifndef EPICS_ATOMIC_SUB_SIZET
#define EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta )
{
    /* 
     * vxAtomicLib doc indicates that vxAtomicSub is 
     * implemented using signed arithmetic, but it
     * does not change the end result because twos
     * complement subtraction is used in either case
     */
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicSub ( pTarg, (atomic_t) delta );
    return ( ( size_t ) oldVal ) - delta;
}
#endif

#ifndef EPICS_ATOMIC_CAS_SIZET
#define EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget, 
                            size_t oldVal, size_t newVal )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return ( size_t ) vxCas ( pTarg, (atomic_t) oldVal, (atomic_t) newVal );
}
#endif

#ifndef EPICS_ATOMIC_CAS_PTRT
#define EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return (EpicsAtomicPtrT) vxCas ( pTarg, (atomic_t) oldVal, (atomic_t) newVal );
}
#endif

#else /* ULONG_MAX == UINT_MAX */

/*
 * if its 64 bit SMP vxWorks and the compiler doesnt
 * have an intrinsic then maybe there isnt any way to 
 * implement these without using a global lock because 
 * size_t is maybe bigger than atomic_t
 *
 * I dont yet have access to vxWorks manuals for 
 * 64 bit systems so this is still undecided, but is
 * defaulting now to a global lock
 */

#endif /* ULONG_MAX == UINT_MAX */

STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( int ) );

#ifndef EPICS_ATOMIC_INCR_INTT
#define EPICS_ATOMIC_INCR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicInc ( pTarg );
    return 1 + ( int ) oldVal;
}
#endif

#ifndef EPICS_ATOMIC_DECR_INTT
#define EPICS_ATOMIC_DECR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicDec ( pTarg );
    return ( ( int ) oldVal ) - 1;
}
#endif

#ifndef EPICS_ATOMIC_ADD_INTT
#define EPICS_ATOMIC_ADD_INTT
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicAdd ( pTarg, (atomic_t) delta );
    return delta + ( int ) oldVal;
}
#endif

#ifndef EPICS_ATOMIC_CAS_INTT
#define EPICS_ATOMIC_CAS_INTT
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget, 
                                            int oldVal, int newVal )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return ( int ) vxCas ( pTarg, (atomic_t) oldVal, (atomic_t) newVal );
}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#else /* _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606 */

#include "vxLib.h"
#include "intLib.h"

#define EPICS_ATOMIC_OS_NAME "VX-INTLIB"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef EPICS_ATOMIC_LOCK
#define EPICS_ATOMIC_LOCK

typedef struct EpicsAtomicLockKey { int m_key; } EpicsAtomicLockKey;

EPICS_ATOMIC_INLINE void epicsAtomicLock ( EpicsAtomicLockKey * pKey )
{
    pKey->m_key = intLock ();
}

EPICS_ATOMIC_INLINE void epicsAtomicUnlock ( EpicsAtomicLockKey * pKey )
{
    intUnlock ( pKey->m_key );
}
#endif

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
#define EPICS_ATOMIC_READ_MEMORY_BARRIER
/* 
 * no need for memory barrior since prior to vxWorks 6.6 it is a single cpu system 
 * (we are not protecting against multiple access to memory mapped IO)
 */
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void) {}
#endif

#ifndef EPICS_ATOMIC_WRITE_MEMORY_BARRIER
#define EPICS_ATOMIC_WRITE_MEMORY_BARRIER
/* 
 * no need for memory barrior since prior to vxWorks 6.6 it is a single cpu system 
 * (we are not protecting against multiple access to memory mapped IO)
 */
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void) {}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606 */

#include "epicsAtomicDefault.h"

#endif /* epicsAtomicOSD_h */

