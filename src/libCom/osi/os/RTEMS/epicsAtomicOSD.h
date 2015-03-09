
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

#include <shareLib.h>
#include "epicsMMIO.h"
#include "compilerSpecific.h"
#include "epicsInterrupt.h"

#define EPICS_ATOMIC_OS_NAME "RTEMS"

typedef struct EpicsAtomicLockKey {
  int key;
} EpicsAtomicLockKey;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    epicsAtomicMemoryBarrierFallback();
}
#endif

#ifndef EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    rwbarr();
}
#endif

EPICS_ATOMIC_INLINE void epicsAtomicLock ( struct EpicsAtomicLockKey * pkey )
{
    pkey->key = epicsInterruptLock();
}

EPICS_ATOMIC_INLINE void epicsAtomicUnlock ( struct EpicsAtomicLockKey * pkey )
{
    epicsInterruptUnlock(pkey->key);
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#include "epicsAtomicDefault.h"

#endif /* epicsAtomicOSD_h */

