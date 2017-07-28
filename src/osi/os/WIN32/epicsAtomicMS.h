
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

#ifndef epicsAtomicMS_h
#define epicsAtomicMS_h

#include "epicsAssert.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef EPICS_ATOMIC_INCR_INTT
#define EPICS_ATOMIC_INCR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicIncrIntT ( int * pTarget )
{
    STATIC_ASSERT ( sizeof ( MS_LONG ) == sizeof ( int ) );
    MS_LONG * const pTarg = ( MS_LONG * ) pTarget;
    return MS_InterlockedIncrement ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_DECR_INTT
#define EPICS_ATOMIC_DECR_INTT
EPICS_ATOMIC_INLINE int epicsAtomicDecrIntT ( int * pTarget )
{
    STATIC_ASSERT ( sizeof ( MS_LONG ) == sizeof ( int ) );
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    return MS_InterlockedDecrement ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_ADD_INTT
#define EPICS_ATOMIC_ADD_INTT
EPICS_ATOMIC_INLINE int epicsAtomicAddIntT ( int * pTarget, int delta )
{
    STATIC_ASSERT ( sizeof ( MS_LONG ) == sizeof ( int ) );
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    /* we dont use InterlockedAdd because only latest windows is supported */
    return delta + ( int ) MS_InterlockedExchangeAdd ( pTarg, 
                                            ( MS_LONG ) delta );
}
#endif

#ifndef EPICS_ATOMIC_CAS_INTT
#define EPICS_ATOMIC_CAS_INTT
EPICS_ATOMIC_INLINE int epicsAtomicCmpAndSwapIntT ( int * pTarget, 
                                            int oldVal, int newVal )
{
    STATIC_ASSERT ( sizeof ( MS_LONG ) == sizeof ( int ) );
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    return (int) MS_InterlockedCompareExchange ( pTarg, 
                                    (MS_LONG) newVal, (MS_LONG) oldVal );
}
#endif

#if ! defined ( MS_ATOMIC_64 )

/*
 * necessary for next three functions 
 *
 * looking at the MS documentation it appears that they will
 * keep type long the same size as an int on 64 bit builds
 */
STATIC_ASSERT ( sizeof ( MS_LONG ) == sizeof ( size_t ) );

#ifndef EPICS_ATOMIC_INCR_SIZET
#define EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    MS_LONG * const pTarg = ( MS_LONG * ) pTarget;
    return MS_InterlockedIncrement ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_DECR_SIZET
#define EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    return MS_InterlockedDecrement ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_ADD_SIZET
#define EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, 
                                                    size_t delta )
{
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    /* we dont use InterlockedAdd because only latest windows is supported */
    return delta + ( size_t ) MS_InterlockedExchangeAdd ( pTarg, 
                                                 ( MS_LONG ) delta );
}
#endif

#ifndef EPICS_ATOMIC_SUB_SIZET
#define EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta )
{
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    MS_LONG ldelta = (MS_LONG) delta;
    /* we dont use InterlockedAdd because only latest windows is supported */
    return ( ( size_t ) MS_InterlockedExchangeAdd ( pTarg, -ldelta ) ) - delta;
}
#endif

#ifndef EPICS_ATOMIC_CAS_SIZET
#define EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( 
                                    size_t * pTarget, 
                                    size_t oldVal, size_t newVal )
{
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    return (size_t) MS_InterlockedCompareExchange ( pTarg, 
                                    (MS_LONG) newVal, (MS_LONG) oldVal );
}
#endif

#ifndef EPICS_ATOMIC_CAS_PTRT
#define EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( 
                                    EpicsAtomicPtrT * pTarget, 
                                    EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    MS_LONG * const pTarg = ( MS_LONG * ) ( pTarget );
    return (EpicsAtomicPtrT) MS_InterlockedCompareExchange ( pTarg, 
                                    (MS_LONG) newVal, (MS_LONG) oldVal );
}
#endif

#else /* ! MS_ATOMIC_64 */

/*
 * necessary for next three functions 
 */
STATIC_ASSERT ( sizeof ( MS_LONGLONG ) == sizeof ( size_t ) );

#ifndef EPICS_ATOMIC_INCR_SIZET
#define EPICS_ATOMIC_INCR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) pTarget;
    return ( size_t ) MS_InterlockedIncrement64 ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_DECR_SIZET
#define EPICS_ATOMIC_DECR_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) ( pTarget );
    return ( size_t ) MS_InterlockedDecrement64 ( pTarg );
}
#endif

#ifndef EPICS_ATOMIC_ADD_SIZET
#define EPICS_ATOMIC_ADD_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicAddSizeT ( size_t * pTarget, size_t delta )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) ( pTarget );
    /* we dont use InterlockedAdd64 because only latest windows is supported */
    return delta + ( size_t ) MS_InterlockedExchangeAdd64 ( pTarg, 
                                        ( MS_LONGLONG ) delta );
}
#endif

#ifndef EPICS_ATOMIC_SUB_SIZET
#define EPICS_ATOMIC_SUB_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicSubSizeT ( size_t * pTarget, size_t delta )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) ( pTarget );
    MS_LONGLONG ldelta = (MS_LONGLONG) delta;
    /* we dont use InterlockedAdd64 because only latest windows is supported */
    return (( size_t ) MS_InterlockedExchangeAdd64 ( pTarg, -ldelta )) - delta;
}
#endif

#ifndef EPICS_ATOMIC_CAS_SIZET
#define EPICS_ATOMIC_CAS_SIZET
EPICS_ATOMIC_INLINE size_t epicsAtomicCmpAndSwapSizeT ( size_t * pTarget, 
                                    size_t oldVal, size_t newVal )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) ( pTarget );
    return (size_t) MS_InterlockedCompareExchange64 ( pTarg, 
                                    (MS_LONGLONG) newVal, 
                                    (MS_LONGLONG) oldVal );
}
#endif

#ifndef EPICS_ATOMIC_CAS_PTRT
#define EPICS_ATOMIC_CAS_PTRT
EPICS_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( 
                            EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    MS_LONGLONG * const pTarg = ( MS_LONGLONG * ) ( pTarget );
    return (EpicsAtomicPtrT) MS_InterlockedCompareExchange64 ( pTarg, 
                                    (MS_LONGLONG) newVal, (MS_LONGLONG) oldVal );
}
#endif

#endif /* ! MS_ATOMIC_64 */

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* ifdef epicsAtomicMS_h */

