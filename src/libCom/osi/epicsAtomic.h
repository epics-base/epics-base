
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

/*
 * fetch target in cache, return new value of target
 */
epicsShareFunc size_t epicsAtomicGetSizeT ( const size_t * pTarget );
epicsShareFunc unsigned epicsAtomicGetUIntT ( const unsigned * pTarget );

/*
 * lock out other smp processors from accessing the target,
 * sync target in cache, if target is zero set target to 
 * non-zero (true) value, flush target in cache, allow out 
 * other smp processors to access the target, return true if 
 * this request changed the target from a zero value to a 
 * non-zero (true) value and otherwise false
 *
 * test and set is chosen as the primitive here because,
 * compared to comapare-and-swap it is more likely to
 * be implemented atomically on old architectures such as 68k
 */
epicsShareFunc unsigned epicsAtomicTestAndSetUIntT ( unsigned * pTarget );

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
epicsShareFunc size_t epicsLockedGetSizeT ( const size_t * pTarget );
epicsShareFunc unsigned epicsLockedGetUIntT ( const unsigned * pTarget );
epicsShareFunc unsigned epicsLockedTestAndSetUIntT ( unsigned * pTarget );

#ifdef __cplusplus
} /* end of extern "C" */
#endif

/*
 * options for inline compiler instrinsic or os specific 
 * implementations of the above function prototypes
 */
#include "epicsAtomicCD.h"

#endif /* epicsAtomic_h */
