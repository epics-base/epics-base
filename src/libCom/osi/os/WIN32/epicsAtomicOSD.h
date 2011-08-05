
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

#define __STDC_LIMIT_MACROS /* define SIZE_MAX for c++ */
#include <stdint.h>

#include "epicsAssert.h"

#define STRICT
#define VC_EXTRALEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* necessary for next two functions */
STATIC_ASSERT ( sizeof ( LONG ) == sizeof ( unsigned ) );

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    InterlockedExchange ( pTarg, ( LONG ) newVal );
}

OSD_ATOMIC_INLINE unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget )
{
    long * const pTarg = ( LONG * ) ( pTarget );
    return InterlockedCompareExchange ( pTarg, 1, 0 ) == 0;
}

/* 
 * on win32 a LONG is 32 bits, but I am concerned that
 * we shouldnt use LONG_MAX here because with certain
 * compilers a long will be 64 bits wide
 */
#define WIN32_LONG_MAX 0xffffffff
#if SIZE_MAX == WIN32_LONG_MAX

/*
 * necessary for next four functions 
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

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    InterlockedExchange ( pTarg, ( LONG ) newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    LONG * const pTarg = ( LONG * ) ( pTarget );
    return InterlockedExchangeAdd ( pTarg, 0 );
}

#else /* SIZE_MAX == WIN32_LONG_MAX */

/*
 * necessary for next four functions 
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

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    LONGLONG * const pTarg = ( LONGLONG * ) ( pTarget );
    InterlockedExchange64 ( pTarg, ( LONGLONG ) newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    LONGLONG * const pTarg = ( LONGLONG * ) ( pTarget );
    return InterlockedExchangeAdd64 ( pTarg, 0 );
}

#endif /* SIZE_MAX == WIN32_LONG_MAX */

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* if defined ( OSD_ATOMIC_INLINE ) */

#endif /* ifndef epicsAtomicOSD_h */
