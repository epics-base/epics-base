
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

struct EpicsAtomicLockKey {};
epicsShareFunc void epicsAtomicReadMemoryBarrier ();
epicsShareFunc void epicsAtomicWriteMemoryBarrier ();
epicsShareFunc void epicsAtomicLock ( struct EpicsAtomicLockKey * );
epicsShareFunc void epicsAtomicUnlock ( struct EpicsAtomicLockKey * );

#ifdef EPICS_ATOMIC_INLINE

#include "epicsAtomicDefault.h"

#endif /* ifdef EPICS_ATOMIC_INLINE */

#endif /* epicsAtomicOSD_h */

