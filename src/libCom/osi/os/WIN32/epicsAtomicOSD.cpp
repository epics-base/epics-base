
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of 
*     Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#define epicsExportSharedSymbols
#include "epicsAtomic.h"

// if the compiler is unable to inline then instantiate out-of-line
#ifndef EPICS_ATOMIC_INLINE
#   define EPICS_ATOMIC_INLINE
#   include "epicsAtomicOSD.h"
#endif

