
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

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

#if defined ( OSD_ATOMIC_INLINE )

/* 
 * atomic.h exists only in Solaris 10 or higher
 */
#if defined ( __SunOS_5_10 )

#include <limits.h>
#include <atomic.h>

#define __STDC_LIMIT_MACROS /* define SIZE_MAX for c++ */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

OSD_ATOMIC_INLINE unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                                        unsigned oldVal, unsigned newVal )
{
    return atomic_cas_uint ( pTarget, oldVal, newVal );
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    return atomic_cas_ptr ( pTarget, oldVal, newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    void * const pTarg = ( void * ) ( pTarget );
    return ( size_t ) atomic_inc_ptr_nv ( pTarg );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    void * const pTarg = ( void * ) ( pTarget );
    return atomic_dec_ptr_nv ( pTarg );
}

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    *pTarget = newVal;
    membar_producer();
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    *pTarget = newVal;
    membar_producer();
}

OSD_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
    *pTarget = newVal;
    membar_producer();
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    membar_consumer ();
    return *pTarget;
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    membar_consumer ();
    return *pTarget;
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    membar_consumer ();
    return *pTarget;
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#else /* ifdef __SunOS_5_10  */

#include "epicsAtomicLocked.h"

#endif /* ifdef __SunOS_5_10 */

#endif /* if defined ( OSD_ATOMIC_INLINE ) */

#endif /* epicsAtomicOSD_h */
