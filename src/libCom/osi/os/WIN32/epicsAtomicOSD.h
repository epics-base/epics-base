
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

#include <limits.h>

#include "epicsAssert.h"

#define STRICT
#define VC_EXTRALEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* 
 * mingw doesnt currently provide MemoryBarrier
 * (this is mostly for testing purposes as the gnu
 * intrinsics are used if compiling for 486 or better 
 * minimum hardware)
 */
#ifdef __MINGW32__
    extern inline void MemoryBarrier() {
        int fence = 0;
        __asm__ __volatile__( "xchgl %%eax,%0 ":"=r" (fence) );
    }
#endif // ifdef __MINGW32__

/* necessary for next three functions */
STATIC_ASSERT ( sizeof ( LONG ) == sizeof ( unsigned ) );

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    *pTarget = newVal;
    MemoryBarrier ();
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    *pTarget = newVal;
    MemoryBarrier ();
}

OSD_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
    *pTarget = newVal;
    MemoryBarrier ();
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    MemoryBarrier ();
    return *pTarget;
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    MemoryBarrier ();
    return *pTarget;
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    MemoryBarrier ();
    return *pTarget;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                                            unsigned oldVal, unsigned newVal )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    return (unsigned) InterlockedCompareExchange ( pTarg, 
                                    (LONG) newVal, (LONG) oldVal );
}

#if defined ( _WIN32 )

/*
 * necessary for next five functions 
 *
 * looking at the MS documentation it appears that they will
 * keep type long the same size as an int on 64 bit builds
 */
STATIC_ASSERT ( sizeof ( LONG ) == sizeof ( size_t ) );

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    LONG * const pTarg = ( LONG * ) pTarget;
    return InterlockedIncrement ( pTarg );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    return InterlockedDecrement ( pTarg );
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    return (EpicsAtomicPtrT) InterlockedCompareExchange ( pTarg, 
                                    (LONG) newVal, (LONG) oldVal );
}

#elif defined ( _WIN64 ) 

/*
 * necessary for next five functions 
 */
STATIC_ASSERT ( sizeof ( LONGLONG ) == sizeof ( size_t ) );

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    LONGLONG * const pTarg = ( LONGLONG * ) pTarget;
    return InterlockedIncrement64 ( pTarg );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    LONGLONG * const pTarg = ( LONGLONG * ) ( pTarget );
    return InterlockedDecrement64 ( pTarg );
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    LONGLONG * const pTarg = ( LONGLONG * ) ( pTarget );
    return (EpicsAtomicPtrT) InterlockedCompareExchange64 ( pTarg, 
                                    (LONGLONG) newVal, (LONGLONG) oldVal );
}

#endif /* if defined ( _WIN32 ) */

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* if defined ( OSD_ATOMIC_INLINE ) */

#endif /* ifndef epicsAtomicOSD_h */
