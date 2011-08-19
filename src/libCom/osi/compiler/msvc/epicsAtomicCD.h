
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

#if _MSC_VER >= 1200
#   define OSD_ATOMIC_INLINE __forceinline
#else
#   define OSD_ATOMIC_INLINE __inline
#endif

#if defined ( _M_IX86 )
#   pragma warning( push )
#   pragma warning( disable : 4793 )
    OSD_ATOMIC_INLINE void OSD_ATOMIC_SYNC ()
    {
        long fence;
        __asm { xchg fence, eax }
    }
#   pragma warning( pop )
#elif defined ( _M_X64 )
#   define OSD_ATOMIC_64
#   pragma intrinsic ( __faststorefence )
#   define OSD_ATOMIC_SYNC __faststorefence
#elif defined ( _M_IA64 )
#   define OSD_ATOMIC_64
#   pragma intrinsic ( __mf )
#   define OSD_ATOMIC_SYNC __mf
#else
#   error unexpected target architecture, msvc version of epicsAtomicCD.h
#endif

#define OSD_ATOMIC_INLINE_DEFINITION

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

/* necessary for next three functions */
STATIC_ASSERT ( sizeof ( long ) == sizeof ( unsigned ) );

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    *pTarget = newVal;
    OSD_ATOMIC_SYNC ();
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    *pTarget = newVal;
    OSD_ATOMIC_SYNC ();
}

OSD_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, 
                                            EpicsAtomicPtrT newVal )
{
    *pTarget = newVal;
    OSD_ATOMIC_SYNC ();
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    OSD_ATOMIC_SYNC ();
    return *pTarget;
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    OSD_ATOMIC_SYNC ();
    return *pTarget;
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    OSD_ATOMIC_SYNC ();
    return *pTarget;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                                            unsigned oldVal, unsigned newVal )
{
    long * const pTarg = ( long * ) ( pTarget );
    return (unsigned) _InterlockedCompareExchange ( pTarg, 
                                    (long) newVal, (long) oldVal );
}

#if ! OSD_ATOMIC_64

/*
 * necessary for next five functions 
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

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                                    EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    long * const pTarg = ( long * ) ( pTarget );
    return (EpicsAtomicPtrT) _InterlockedCompareExchange ( pTarg, 
                                    (long) newVal, (long) oldVal );
}

#else /* ! OSD_ATOMIC_64 */

/*
 * necessary for next five functions 
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

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    long long * const pTarg = ( longlong * ) ( pTarget );
    return (EpicsAtomicPtrT) _InterlockedCompareExchange64 ( pTarg, 
                                    (long long) newVal, (long long) oldVal );
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
