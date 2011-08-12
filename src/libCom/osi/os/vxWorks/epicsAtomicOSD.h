/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne, LLC, as Operator of 
*     Argonne National Laboratory.
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

#include "vxWorks.h" /* obtain the version of vxWorks */
#include "epicsAssert.h"

/*
 * With vxWorks 6.6 and later we need to use vxAtomicLib
 * to implement this functionality correctly on SMP systems
 */
#if _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606

#include <limits.h>
#include <vxAtomicLib.h>
     
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * we make the probably correct guess that if ULONG_MAX 
 * is the same as UINT_MAX then sizeof ( atomic_t )
 * will be the same as sizeof ( size_t )
 *
 * if ULONG_MAX != UINT_MAX then its 64 bit vxWorks and
 * WRS doesnt not supply at this time the atomic interface 
 * for 8 byte integers that is needed - so that architecture 
 * receives the lock synchronized version
 */
#if ULONG_MAX == UINT_MAX

STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( size_t ) );

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    /* 
     * vxAtomicLib doc indicates that vxAtomicInc is 
     * implemented using unsigned arithmetic
     */
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicInc ( pTarg ) + 1;
    return ( ( size_t ) ( oldVal ) ) + 1u;
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    /* 
     * vxAtomicLib doc indicates that vxAtomicDec is 
     * implemented using unsigned arithmetic
     */
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    const atomic_t oldVal = vxAtomicDec ( pTarg ) - 1;
    return ( ( size_t ) ( oldVal ) ) - 1u;
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    vxAtomicSet ( pTarg, newVal ); 
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return ( size_t ) vxAtomicGet ( pTarg );
}

#else /* SIZE_MAX == UINT_MAX */

/*
 * if its 64 bit vxWorks and the compiler doesnt
 * have an intrinsic then maybe there isnt any way to 
 * implement these without using a global lock because 
 * size_t is bigger than atomic_t
 */
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

#endif /* SIZE_MAX == UINT_MAX */

STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( unsigned ) );

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    vxAtomicSet ( pTarg, newVal ); 
}

OSD_ATOMIC_INLINE unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget )
{
    STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( unsigned ) );
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return vxCas ( pTarg, 0, 1 ) != 0;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    STATIC_ASSERT ( sizeof ( atomic_t ) == sizeof ( unsigned ) );
    atomic_t * const pTarg = ( atomic_t * ) ( pTarget );
    return ( unsigned ) vxAtomicGet ( pTarg );
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#else /* _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606 */

#include "vxLib.h"
#include "intLib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

OSD_ATOMIC_INLINE size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
#   ifdef __m68k__
        return ++(*pTarget);
#   else
      /* 
       * no need for memory barrior since this 
       * is a single cpu system.
       */
      const int key = intLock ();
      const size_t result = ++(*pTarget);
      intUnlock ( key );
      return result;
#   endif
}

OSD_ATOMIC_INLINE size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
#   ifdef __m68k__
        return --(*pTarget);
#   else
      /* 
       * no need for memory barrior since this 
       * is a single cpu system 
       */
      const int key = intLock ();
      const size_t result = --(*pTarget);
      intUnlock ( key );
      return result;
#endif
}

OSD_ATOMIC_INLINE void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    /* 
     * no need for memory barrior since this 
     * is a single cpu system 
     */
    *pTarget = newVal; 
}

OSD_ATOMIC_INLINE void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    /* 
     * no need for memory barrior since this 
     * is a single cpu system 
     */
    *pTarget = newVal; 
}

OSD_ATOMIC_INLINE size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    /* 
     * no need for memory barrior since this 
     * is a single cpu system 
     */
    return *pTarget;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    /* 
     * no need for memory barrior since this 
     * is a single cpu system 
     */
    return *pTarget;
}

OSD_ATOMIC_INLINE unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget )
{
    STATIC_ASSERT ( sizeof ( int ) == sizeof ( unsigned ) );
    return vxTas ( pTarget );
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#endif /* _WRS_VXWORKS_MAJOR * 100 + _WRS_VXWORKS_MINOR >= 606 */

#endif /* if defined ( OSD_ATOMIC_INLINE ) */

#endif /* epicsAtomicOSD_h */
