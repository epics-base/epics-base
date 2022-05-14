
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

#include <libComAPI.h>

#define EPICS_ATOMIC_OS_NAME "POSIX"

typedef struct EpicsAtomicLockKey {} EpicsAtomicLockKey;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

LIBCOM_API void epicsAtomicLock ( struct EpicsAtomicLockKey * );
LIBCOM_API void epicsAtomicUnlock ( struct EpicsAtomicLockKey * );
LIBCOM_API void epicsAtomicMemoryBarrierFallback ( void );

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    epicsAtomicMemoryBarrierFallback();
}
#endif

#ifndef EPICS_ATOMIC_WRITE_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    epicsAtomicMemoryBarrierFallback();
}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#include "epicsAtomicDefault.h"

#endif /* epicsAtomicOSD_h */

