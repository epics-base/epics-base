
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

#if defined ( __cplusplus )
#   define EPICS_ATOMIC_INLINE inline
#else
#   define EPICS_ATOMIC_INLINE __inline__
#endif

/* 
 * we have an inline keyword so we can proceed
 * with an os specific inline instantiation
 */
#include "epicsAtomicOSD.h"

#endif /* epicsAtomicCD_h */
