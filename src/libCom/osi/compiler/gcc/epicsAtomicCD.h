
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

#ifndef epicsAtomicCD_h
#define epicsAtomicCD_h

#ifndef __GNUC__
#   error this header is only for use with the gnu compiler
#endif

#define OSD_ATOMIC_INLINE __inline__

#define GCC_ATOMIC_CONCAT( A, B ) GCC_ATOMIC_CONCATR(A,B)
#define GCC_ATOMIC_CONCATR( A, B ) A ## B

#define GCC_ATOMIC_INTRINSICS_AVAIL_UINT_T \
    GCC_ATOMIC_CONCAT ( \
        __GCC_HAVE_SYNC_COMPARE_AND_SWAP_, \
        __SIZEOF_INT__ )

#define GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T \
    GCC_ATOMIC_CONCAT ( \
        __GCC_HAVE_SYNC_COMPARE_AND_SWAP_, \
        __SIZEOF_SIZE_T__ )
        
#define GCC_ATOMIC_INTRINSICS_AVAIL_EARLIER \
    defined ( __i386 ) && ( __GNUC__ * 100 + __GNUC_MINOR__ ) >= 401

#if ( GCC_ATOMIC_INTRINSICS_AVAIL_UINT_T \
    && GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T ) \
    || GCC_ATOMIC_INTRINSICS_AVAIL_EARLIER

#define OSD_ATOMIC_GCC

#ifdef __cplusplus
extern "C" {
#endif

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    return __sync_add_and_fetch ( pTarget, 1u );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    return __sync_sub_and_fetch ( pTarget, 1u );
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, 
                                        size_t newValue )
{
    *pTarget = newValue;
    __sync_synchronize ();
}

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, 
                                        unsigned newValue )
{
    *pTarget = newValue;
    __sync_synchronize ();
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    __sync_synchronize ();
    return *pTarget;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget )
{
    const size_t prev = __sync_lock_test_and_set ( pTarget, 1u );
    return prev == 0;
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#else /* if GCC_ATOMIC_INTRINSICS_AVAIL */
    
    /*
     * not available as gcc intrinsics so we
     * will employ an os specific inline solution
     */
#   include "epicsAtomicOSD.h"

#endif /* if GCC_ATOMIC_INTRINSICS_AVAIL */

#endif /* epicsAtomicCD_h */
