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
 
//
// The epicsAtomicXxxxx functions herein are implemented with C++ but directly 
// callable by C code, and therefore they must not be instantiated inline. 
// Therefore, this isnt a traditional header file because it has function
// definitions that are not inline, and must therefore be included by only 
// one module in the executable - typically this is the epicsAtomicOSD
// c++ source file. These out-of-line function definitions are placed in a
// header file so that there is potential code reuse when instantiating for 
// a specific OS. An alternative option would have been to place these 
// function definitions in a OS type conditionally compiled source file
// but this wasnt done because I suspect that selecting the posix version
// would require a proliferation of "SRCS_XXXX += xxx.cpp" in the libCom 
// Makefile for every variety of Unix (a maintenance headache when new 
// varieties of UNIX are configured).
//
// Aee also the comments in epicsAtomicGuard header.
//

#ifndef epicsAtomicLocked_h
#define epicsAtomicLocked_h

#ifndef __cplusplus
#   error epicsAtomicLocked.h is intended only for use only by C++ code
#endif

#include "epicsAtomic.h" // always cross check the function prototypes
#include "epicsAtomicGuard.h" 

extern "C" {

size_t epicsAtomicIncrSizeT ( size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return ++(*pTarget);
}

size_t epicsAtomicDecrSizeT ( size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return --(*pTarget);
}

void epicsAtomicSetSizeT ( size_t * pTarget, size_t newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

void epicsAtomicSetUIntT ( unsigned * pTarget, unsigned newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

void epicsAtomicSetPtrT ( EpicsAtomicPtrT * pTarget, EpicsAtomicPtrT newVal )
{
    AtomicGuard atomicGuard;
    *pTarget = newVal;
}

unsigned epicsAtomicGetUIntT ( const unsigned * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

size_t epicsAtomicGetSizeT ( const size_t * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

EpicsAtomicPtrT epicsAtomicGetPtrT ( const EpicsAtomicPtrT * pTarget )
{
    AtomicGuard atomicGuard;
    return *pTarget;
}

unsigned epicsAtomicCmpAndSwapUIntT ( unsigned * pTarget, 
                            unsigned oldval, unsigned newval )
{
    AtomicGuard atomicGuard;
    const unsigned cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    return cur;
}

EpicsAtomicPtrT epicsAtomicCmpAndSwapPtrT ( EpicsAtomicPtrT * pTarget, 
                            EpicsAtomicPtrT oldval, EpicsAtomicPtrT newval )
{
    AtomicGuard atomicGuard;
    const EpicsAtomicPtrT cur = *pTarget;
    if ( cur == oldval ) {
        *pTarget = newval;
    }
    return cur;
}

} // end of extern "C"

#endif /* epicsAtomicLocked_h */

