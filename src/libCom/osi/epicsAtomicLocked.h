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

#ifndef epicsAtomicLocked_h
#define epicsAtomicLocked_h

#if defined ( OSD_ATOMIC_INLINE )

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    return epicsLockedIncrSizeT ( pTarget );
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    return epicsLockedDecrSizeT ( pTarget );
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    epicsLockedSetSizeT ( pTarget, newVal );
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    return epicsLockedGetSizeT ( pTarget );
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    return epicsLockedGetUIntT ( pTarget );
}

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    epicsLockedSetUIntT ( pTarget, newVal );
}

OSD_ATOMIC_INLINE void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
    epicsLockedSetPtrT ( pTarget, newVal );
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    return epicsLockedGetPtrT ( pTarget );
}

OSD_ATOMIC_INLINE unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                                        unsigned oldVal, unsigned newVal )
{
    return epicsLockedCmpAndSwapUIntT ( pTarget, oldVal, newVal );
}

OSD_ATOMIC_INLINE EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                                        EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal )
{
    return epicsLockedCmpAndSwapPtrT ( pTarget, oldVal, newVal );
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#else /* if defined ( OSD_ATOMIC_INLINE ) */

#   define epicsAtomicIncrSizeT epicsLockedIncrSizeT
#   define epicsAtomicDecrSizeT epicsLockedDecrSizeT
#   define epicsAtomicSetSizeT epicsLockedSetSizeT
#   define epicsAtomicSetUIntT epicsLockedSetUIntT
#   define epicsAtomicSetPtrT epicsLockedSetPtrT
#   define epicsAtomicGetSizeT epicsLockedGetSizeT
#   define epicsAtomicGetUIntT epicsLockedGetUIntT
#   define epicsAtomicGetPtrT epicsLockedGetPtrT
#   define epicsAtomicCmpAndSwapUIntT epicsLockedCmpAndSwapUIntT
#   define epicsAtomicCmpAndSwapPtrT epicsLockedCmpAndSwapPtrT

#endif /* if defined ( OSD_ATOMIC_INLINE ) */

#endif /* epicsAtomicLocked_h */

