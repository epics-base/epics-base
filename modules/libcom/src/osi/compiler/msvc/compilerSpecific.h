/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:
 * Jeffrey O. Hill
 * johill@lanl.gov
 */

#ifndef compilerSpecific_h
#define compilerSpecific_h

#ifndef _MSC_VER
#   error compiler/msvc/compilerSpecific.h is only for use with the Microsoft compiler
#endif

#define EPICS_ALWAYS_INLINE __forceinline

/* Expands to a 'const char*' which describes the name of the current function scope */
#define EPICS_FUNCTION __FUNCTION__

#ifdef __cplusplus

/*
 * in general we don't like ifdefs but they do allow us to check the
 * compiler version and make the optimistic assumption that
 * standards incompliance issues will be fixed by future compiler
 * releases
 */

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */
#define CXX_PLACEMENT_DELETE
#define CXX_THROW_SPECIFICATION

#endif /* __cplusplus */

/*
 * Enable format-string checking if compiler supports it (if msvc is 2015 or newer)
 */
#if _MSC_VER >= 1900
#   define EPICS_PRINTF_FMT(a) _Printf_format_string_ a
#endif

#endif  /* ifndef compilerSpecific_h */
