
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

#ifndef epicsAtomicCD_h
#define epicsAtomicCD_h

#ifndef __clang__
#   error this header is only for use with the Clang compiler
#endif

#define EPICS_ATOMIC_CMPLR_NAME "CLANG"

#include <epicsAtomicGCC.h>

/*
 * if currently unavailable as intrinsics we
 * will try for an os specific inline solution
 */
#include "epicsAtomicOSD.h"

#endif /* epicsAtomicCD_h */
