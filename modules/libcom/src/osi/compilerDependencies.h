/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file   compilerDependencies.h
 * \author Jeffrey O. Hill johill@lanl.gov
 * \brief  Compiler specific declarations
 *
 */

#ifndef compilerDependencies_h
#define compilerDependencies_h

#include "compilerSpecific.h"

#ifdef __cplusplus

/*
 * usage: epicsPlacementDeleteOperator (( void *, myMemoryManager & ))
 */
#if defined ( CXX_PLACEMENT_DELETE )
#   define epicsPlacementDeleteOperator(X) void operator delete X;
#else
#   define epicsPlacementDeleteOperator(X)
#endif

#endif /* __cplusplus */


#ifndef EPICS_PRINTF_STYLE
/*
 * No format-string checking
 */
#   define EPICS_PRINTF_STYLE(f,a)
#endif

#ifndef EPICS_SIZED_MEM_CHECK
#   define EPICS_SIZED_MEM_CHECK(...)
#endif

#ifndef EPICS_MEM_CHECK
#   define EPICS_MEM_CHECK(freeMem, freeMemArg)
#endif

#ifndef EPICS_NON_NULL_PTR_ARGS
#   define EPICS_NON_NULL_PTR_ARGS(...)
#endif

#ifndef EPICS_DEPRECATED
/*
 * No deprecation markers
 */
#define EPICS_DEPRECATED
#endif

#ifndef EPICS_UNUSED
#   define EPICS_UNUSED
#endif

#ifndef EPICS_FUNCTION
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)) || (defined(__cplusplus) && __cplusplus>=201103L)
#  define EPICS_FUNCTION __func__
#else
/* Expands to a 'const char*' which describes the name of the current function scope */
#  define EPICS_FUNCTION ("<unknown function>")
#endif
#endif

#endif  /* ifndef compilerDependencies_h */
