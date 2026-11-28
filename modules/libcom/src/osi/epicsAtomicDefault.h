
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

#ifndef epicsAtomicDefault_h
#define epicsAtomicDefault_h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * struct EpicsAtomicLockKey;
 * LIBCOM_API void epicsAtomicReadMemoryBarrier ();
 * LIBCOM_API void epicsAtomicWriteMemoryBarrier ();
 * LIBCOM_API void epicsAtomicLock ( struct EpicsAtomicLockKey * );
 * LIBCOM_API void epicsAtomicUnock ( struct EpicsAtomicLockKey * );
 */

/*
 * incr
 */
#ifndef EPICS_ATOMIC_INCR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget )
{
    EpicsAtomicLockKey key;
    int result;

    epicsAtomicLock ( & key );
    result = ++(*pTarget);
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

#ifndef EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    EpicsAtomicLockKey key;
    size_t result;

    epicsAtomicLock ( & key );
    result = ++(*pTarget);
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

/*
 * decr
 */
#ifndef EPICS_ATOMIC_DECR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget )
{
    EpicsAtomicLockKey key;
    int result;

    epicsAtomicLock ( & key );
    result = --(*pTarget);
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

#ifndef EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    EpicsAtomicLockKey key;
    size_t result;

    epicsAtomicLock ( & key );
    result = --(*pTarget);
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

/*
 * add/sub
 */
#ifndef EPICS_ATOMIC_ADD_INTT
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta )
{
    EpicsAtomicLockKey key;
    int result;

    epicsAtomicLock ( & key );
    result = *pTarget += delta;
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

#ifndef EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta )
{
    EpicsAtomicLockKey key;
    size_t result;

    epicsAtomicLock ( & key );
    result = *pTarget += delta;
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

#ifndef EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta )
{
    EpicsAtomicLockKey key;
    size_t result;

    epicsAtomicLock ( & key );
    result = *pTarget -= delta;
    epicsAtomicUnlock ( & key );
    return result;
}
#endif

/*
 * set
 */
#ifndef EPICS_ATOMIC_SET_INTT
EPICS_ATOMIC_INLINE void epicsAtomicSetIntT ( int * pTarget, int newVal )
{
    *pTarget = newVal;
    epicsAtomicWriteMemoryBarrier ();
}
#endif

#ifndef EPICS_ATOMIC_SET_SIZET
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    *pTarget = newVal;
    epicsAtomicWriteMemoryBarrier ();
}
#endif

#ifndef EPICS_ATOMIC_SET_PTRT
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget,
                                                EpicsAtomicPtrT newVal )
{
    *pTarget = newVal;
    epicsAtomicWriteMemoryBarrier ();
}
#endif

/*
 * get
 */
#ifndef EPICS_ATOMIC_GET_INTT
EPICS_ATOMIC_INLINE int epicsAtomicGetIntT ( const int * pTarget )
{
    epicsAtomicReadMemoryBarrier ();
    return *pTarget;
}
#endif

#ifndef EPICS_ATOMIC_GET_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    epicsAtomicReadMemoryBarrier ();
    return *pTarget;
}
#endif

#ifndef EPICS_ATOMIC_GET_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT
        epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    epicsAtomicReadMemoryBarrier ();
    return *pTarget;
}
#endif

/*
 * cmp and swap
 */
#ifndef EPICS_ATOMIC_CAS_INTT
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget, int oldval, int newval )
{
    EpicsAtomicLockKey key;
    int cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    epicsAtomicUnlock ( & key );
    return cur;
}
#endif

#ifndef EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget,
                                size_t oldval, size_t newval )
{
    EpicsAtomicLockKey key;
    size_t cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    epicsAtomicUnlock ( & key );
    return cur;
}
#endif

#ifndef EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT (
                            EpicsAtomicPtrT * pTarget,
                            EpicsAtomicPtrT oldval, EpicsAtomicPtrT newval )
{
    EpicsAtomicLockKey key;
    EpicsAtomicPtrT cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    epicsAtomicUnlock ( & key );
    return cur;
}
#endif

/*
 * exchange
 *
 * Use a compare-and-swap loop if CAS is lock-free, otherwise the lock.
 */
#ifndef EPICS_ATOMIC_EXCHANGE_INTT
EPICS_ATOMIC_INLINE int epicsAtomicExchangeIntT ( int * pTarget, int newVal )
{
#ifdef EPICS_ATOMIC_CAS_INTT
    int cur = *( volatile int * ) pTarget;
    int prev;
    while ( ( prev = epicsAtomicCmpAndSwapIntT ( pTarget, cur, newVal ) ) != cur ) {
        cur = prev;
    }
    return cur;
#else
    EpicsAtomicLockKey key;
    int cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    *pTarget = newVal;
    epicsAtomicUnlock ( & key );
    return cur;
#endif
}
#endif

#ifndef EPICS_ATOMIC_EXCHANGE_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicExchangeSizeT ( size_t * pTarget,
                                                      size_t newVal )
{
#ifdef EPICS_ATOMIC_CAS_SIZET
    size_t cur = *( volatile size_t * ) pTarget;
    size_t prev;
    while ( ( prev = epicsAtomicCmpAndSwapSizeT ( pTarget, cur, newVal ) ) != cur ) {
        cur = prev;
    }
    return cur;
#else
    EpicsAtomicLockKey key;
    size_t cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    *pTarget = newVal;
    epicsAtomicUnlock ( & key );
    return cur;
#endif
}
#endif

#ifndef EPICS_ATOMIC_EXCHANGE_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicExchangePtrT (
                            EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
#ifdef EPICS_ATOMIC_CAS_PTRT
    EpicsAtomicPtrT cur = *( EpicsAtomicPtrT volatile * ) pTarget;
    EpicsAtomicPtrT prev;
    while ( ( prev = epicsAtomicCmpAndSwapPtrT ( pTarget, cur, newVal ) ) != cur ) {
        cur = prev;
    }
    return cur;
#else
    EpicsAtomicLockKey key;
    EpicsAtomicPtrT cur;

    epicsAtomicLock ( & key );
    cur = *pTarget;
    *pTarget = newVal;
    epicsAtomicUnlock ( & key );
    return cur;
#endif
}
#endif

/*
 * relaxed get/set
 *
 * Aligned int, size_t and pointer accesses are atomic on all supported
 * targets; volatile keeps the compiler from tearing, merging or
 * eliding them.
 */
#ifndef EPICS_ATOMIC_GET_INTT_RELAXED
EPICS_ATOMIC_INLINE int epicsAtomicGetIntTRelaxed ( const int * pTarget )
{
    return *( const volatile int * ) pTarget;
}
#endif

#ifndef EPICS_ATOMIC_GET_SIZET_RELAXED
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeTRelaxed ( const size_t * pTarget )
{
    return *( const volatile size_t * ) pTarget;
}
#endif

#ifndef EPICS_ATOMIC_GET_PTRT_RELAXED
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrTRelaxed (
                            const EpicsAtomicPtrT * pTarget )
{
    return *( EpicsAtomicPtrT const volatile * ) pTarget;
}
#endif

#ifndef EPICS_ATOMIC_SET_INTT_RELAXED
EPICS_ATOMIC_INLINE void epicsAtomicSetIntTRelaxed ( int * pTarget, int newVal )
{
    *( volatile int * ) pTarget = newVal;
}
#endif

#ifndef EPICS_ATOMIC_SET_SIZET_RELAXED
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeTRelaxed ( size_t * pTarget,
                                                      size_t newVal )
{
    *( volatile size_t * ) pTarget = newVal;
}
#endif

#ifndef EPICS_ATOMIC_SET_PTRT_RELAXED
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrTRelaxed ( EpicsAtomicPtrT * pTarget,
                                                     EpicsAtomicPtrT newVal )
{
    *( EpicsAtomicPtrT volatile * ) pTarget = newVal;
}
#endif

/*
 * acquire get, release set
 *
 * An acquire load must not be reordered with any later load or store,
 * a release store not with any earlier load or store. The read and
 * write barriers are only specified to order loads and stores
 * respectively, so both are used.
 */
#ifndef EPICS_ATOMIC_GET_INTT_ACQUIRE
EPICS_ATOMIC_INLINE int epicsAtomicGetIntTAcquire ( const int * pTarget )
{
    int val = *( const volatile int * ) pTarget;
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    return val;
}
#endif

#ifndef EPICS_ATOMIC_GET_SIZET_ACQUIRE
EPICS_ATOMIC_INLINE size_t epicsAtomicGetSizeTAcquire ( const size_t * pTarget )
{
    size_t val = *( const volatile size_t * ) pTarget;
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    return val;
}
#endif

#ifndef EPICS_ATOMIC_GET_PTRT_ACQUIRE
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrTAcquire (
                            const EpicsAtomicPtrT * pTarget )
{
    EpicsAtomicPtrT val = *( EpicsAtomicPtrT const volatile * ) pTarget;
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    return val;
}
#endif

#ifndef EPICS_ATOMIC_SET_INTT_RELEASE
EPICS_ATOMIC_INLINE void epicsAtomicSetIntTRelease ( int * pTarget, int newVal )
{
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    *( volatile int * ) pTarget = newVal;
}
#endif

#ifndef EPICS_ATOMIC_SET_SIZET_RELEASE
EPICS_ATOMIC_INLINE void epicsAtomicSetSizeTRelease ( size_t * pTarget,
                                                      size_t newVal )
{
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    *( volatile size_t * ) pTarget = newVal;
}
#endif

#ifndef EPICS_ATOMIC_SET_PTRT_RELEASE
EPICS_ATOMIC_INLINE void epicsAtomicSetPtrTRelease ( EpicsAtomicPtrT * pTarget,
                                                     EpicsAtomicPtrT newVal )
{
    epicsAtomicReadMemoryBarrier ();
    epicsAtomicWriteMemoryBarrier ();
    *( EpicsAtomicPtrT volatile * ) pTarget = newVal;
}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* epicsAtomicDefault_h */


