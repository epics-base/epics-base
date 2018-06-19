
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
 
#ifndef epicsAtomicDefault_h
#define epicsAtomicDefault_h

#ifdef __cpluplus
extern "C" {
#endif

/*
 * struct EpicsAtomicLockKey;
 * epicsShareFunc void epicsAtomicReadMemoryBarrier ();
 * epicsShareFunc void epicsAtomicWriteMemoryBarrier ();
 * epicsShareFunc void epicsAtomicLock ( struct EpicsAtomicLockKey * );
 * epicsShareFunc void epicsAtomicUnock ( struct EpicsAtomicLockKey * );
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

#ifdef __cpluplus
} /* end of extern "C" */
#endif

#endif /* epicsAtomicDefault_h */


