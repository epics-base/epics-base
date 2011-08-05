
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

#include "epicsAssert.h"

#ifndef _MSC_VER
#   error this header file is only for use with with the Microsoft Compiler
#endif

#ifdef _MSC_EXTENSIONS

/*
 * I have discovered an anomaly in visual c++ where
 * the DLL instantiation of an exported inline interface
 * does not occur in a c++ code unless "inline" is used.
 */
#if defined ( __cplusplus )
#   define OSD_ATOMIC_INLINE inline
#elif defined ( _MSC_VER )
#   define OSD_ATOMIC_INLINE __inline
#endif

#if defined ( _M_X64 ) || defined ( _M_IA64 )
#   define OSD_ATOMIC_64
#endif /* defined ( _M_X64 ) || defined ( _M_IA64 ) */

#include <intrin.h>

#pragma intrinsic ( _InterlockedExchange )
#pragma intrinsic ( _InterlockedCompareExchange )
#pragma intrinsic ( _InterlockedIncrement )
#pragma intrinsic ( _InterlockedDecrement )
#pragma intrinsic ( _InterlockedExchange )
#pragma intrinsic ( _InterlockedExchangeAdd )
#if OSD_ATOMIC_64
#   pragma intrinsic ( _InterlockedIncrement64 )
#   pragma intrinsic ( _InterlockedDecrement64 )
#   pragma intrinsic ( _InterlockedExchange64 )
#   pragma intrinsic ( _InterlockedExchangeAdd64 )
#endif

/*
 * The windows doc appears to recommend defining InterlockedExchange
 * to be _InterlockedExchange to cause it to be an intrinsic, but that
 * creates issues when later, in a windows os specific header, we include
 * windows.h so we except some code duplication between the msvc csAtomic.h
 * and win32 osdAtomic.h to avoid problems, and to keep the os specific 
 * windows.h header file out of the msvc cdAtomic.h
 */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* necessary for next two functions */
STATIC_ASSERT ( sizeof ( long ) == sizeof ( unsigned ) );

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    long * const pTarg = ( long * ) ( pTarget );
    _InterlockedExchange ( pTarg, ( long ) newVal );
}

OSD_ATOMIC_INLINE unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget )
{
    long * const pTarg = ( long * ) ( pTarget );
    return _InterlockedCompareExchange ( pTarg, 1, 0 ) == 0;
}


#if ! OSD_ATOMIC_64

/*
 * necessary for next four functions 
 *
 * looking at the MS documentation it appears that they will
 * keep type long the same size as an int on 64 bit builds
 */
STATIC_ASSERT ( sizeof ( long ) == sizeof ( size_t ) );

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    long * const pTarg = ( long * ) pTarget;
    return _InterlockedIncrement ( pTarg );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    long * const pTarg = ( long * ) ( pTarget );
    return _InterlockedDecrement ( pTarg );
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    long * const pTarg = ( long * ) ( pTarget );
    _InterlockedExchange ( pTarg, ( long ) newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    long * const pTarg = ( long * ) ( pTarget );
    return _InterlockedExchangeAdd ( pTarg, 0 );
}

#else /* ! OSD_ATOMIC_64 */

/*
 * necessary for next four functions 
 */
STATIC_ASSERT ( sizeof ( long long ) == sizeof ( size_t ) );

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    long long * const pTarg = ( long long * ) pTarget;
    return _InterlockedIncrement64 ( pTarg );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    long long * const pTarg = ( long long * ) ( pTarget );
    return _InterlockedDecrement64 ( pTarg );
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    long long * const pTarg = ( long long * ) ( pTarget );
    _InterlockedExchange64 ( pTarg, ( long long ) newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    long long * const pTarg = ( long long * ) ( pTarget );
    return _InterlockedExchangeAdd64 ( pTarg, 0 );
}

#endif /* ! OSD_ATOMIC_64 */

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#else /* ifdef _MSC_EXTENSIONS */

#if defined ( __cplusplus )
#   define OSD_ATOMIC_INLINE inline
#   include "epicsAtomicOSD.h"
#endif

#endif /* ifdef _MSC_EXTENSIONS */

#endif /* epicsAtomicCD_h */
