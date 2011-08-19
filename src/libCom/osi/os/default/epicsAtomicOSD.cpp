
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

//
// We need a default epicsAtopmicOSD.cpp which is empty for use when 
// a compiler or inline OS specific implementation is provided. See 
// "osi/os/posix/epicsAtomicOSD.cpp" for an example OS specific generic 
// out-of-line implementation based on a mutex lock.
//
#if ! defined ( OSD_ATOMIC_INLINE_DEFINITION )
#   error the epicsAtomicXxxxx functions definitions are misssing
#endif

