
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

#ifndef epicsAtomic_h
#define epicsAtomic_h

#include <stdlib.h> /* define size_t */

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * EpicsAtomicPtrT;

/*
 * lock out other smp processors from accessing the target,
 * sync target in cache, add one to target, flush target in 
 * cache, allow other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * increment is chosen as the primitive here because,
 * compared to add, it is more likely to be implemented 
 * atomically on old architectures such as 68k
 */
epicsShareFunc size_t epicsAtomicIncrSizeT ( size_t * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * sync target in cache, subtract one from target, flush target 
 * in cache, allow out other smp processors to access the target,
 * return new value of target as modified by this operation
 *
 * decrement is chosen as the primitive here because,
 * compared to subtract, it is more likely to be implemented 
 * atomically on old architectures such as 68k
 */
epicsShareFunc size_t epicsAtomicDecrSizeT ( size_t * pTarget );

/*
 * set target in cache, flush target in cache
 */
epicsShareFunc void epicsAtomicSetSizeT  ( size_t * pTarget, size_t newValue ); 
epicsShareFunc void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newValue );
epicsShareFunc void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newValue );

/*
 * fetch target in cache, return new value of target
 */
epicsShareFunc size_t epicsAtomicGetSizeT ( const size_t * pTarget );
epicsShareFunc unsigned epicsAtomicGetUIntT ( const unsigned * pTarget );
epicsShareFunc EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * sync target in cache, if target is equal to oldVal set target 
 * to newVal, flush target in cache, allow other smp processors 
 * to access the target, return the original value stored in the
 * target
 *
 * !!!! beware, this will not guarantee an atomic operation
 * !!!! on legacy vxWorks (prior to 6.6) if the target operand 
 * !!!! is on a multi-commander bus (such as the VME bus)
 */
epicsShareFunc unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                                        unsigned oldVal, unsigned newVal );
epicsShareFunc EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal );


/*
 * the following are, never inline and always synchronized by a global 
 * mutual exclusion lock, implementations of the epicsAtomicXxxx interface
 * which may used to implement the epicsAtomicXxxx functions when 
 * more efficent primitives aren't available
 */
epicsShareFunc size_t epicsLockedIncrSizeT ( size_t * pTarget );
epicsShareFunc size_t epicsLockedDecrSizeT ( size_t * pTarget );
epicsShareFunc void epicsLockedSetSizeT ( size_t * pTarget, size_t newVal );
epicsShareFunc void epicsLockedSetUIntT ( unsigned * pTarget, unsigned newVal );
epicsShareFunc void epicsLockedSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal );
epicsShareFunc size_t epicsLockedGetSizeT ( const size_t * pTarget );
epicsShareFunc unsigned epicsLockedGetUIntT ( const unsigned * pTarget );
epicsShareFunc EpicsAtomicPtrT epicsLockedGetPtrT ( const EpicsAtomicPtrT * pTarget );
epicsShareFunc unsigned epicsLockedCmpAndSwapUIntT ( unsigned * pTarget, 
                                        unsigned oldval, unsigned newval );
epicsShareFunc EpicsAtomicPtrT epicsLockedCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldVal, EpicsAtomicPtrT newVal );

#ifdef __cplusplus
} /* end of extern "C" */
#endif

/*
 * options for inline compiler instrinsic or os specific 
 * implementations of the above function prototypes
 */
#include "epicsAtomicCD.h"

#endif /* epicsAtomic_h */
