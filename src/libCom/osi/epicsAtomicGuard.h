
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
 
#ifndef epicsAtomicGuard_h
#define epicsAtomicGuard_h

//
// This header file is intended only for use with the epicsAtomicLocked
// header file. The intent is that an OS specific implementation of
// epicsAtomicOSD.cpp might first include this file, next implement 
// AtomicGuard depending on os specific capabilities, and finally include 
// the epicsAtomicLocked header file to define the various epicsAtomicXxxx 
// functions. 
//
// The AtomicGuard class is defined in a seperate header file 
// from the epicsAtomicLocked header for two reasons. 
// o First, it must be possible to have an inline definition of the 
// AtomicGuard member functions, and that isnt possible if the
// epicsAtomicXxxx function definitions in the epicsAtomicLocked
// header file have already been seen by the translation unit.
// o Second, we need to enforce a uniform interface for all definitions
// of the the AtomicGuard constructor and destructor member functions
// requiring that no exception are thrown. This is because the 
// epicsAtomicXxxx functions in the epicsAtomicLocked header file are 
// directly callable by C code.
//
// The epicsAtomicXxxx functions in the epicsAtomicLocked do not pass
// any parameters to the AtomicGuard constructor, and therefore the lock
// used by AtomicGuard is restricted to be a global lock for the entire
// multithreaded executable. Therefore, AtomicGuard will typically
// reference some translation unit scope limited global variable in 
// the anonymous namespace for implementation of the locking primitive.
//
    
namespace {
    struct AtomicGuard {
        AtomicGuard () throw (); 
        ~AtomicGuard () throw ();
    };
} // end of anonymous namespace

#endif // ifndef epicsAtomicGuard_h

