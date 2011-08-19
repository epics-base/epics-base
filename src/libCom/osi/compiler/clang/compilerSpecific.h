
/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
 
#ifndef __clang__
#   error compiler/clang/compilerSpecific.h is only for use with the clang compiler
#endif

/*
 * WARNING: the current state of this file is only based on reading clang manuals
 * and has not actually been tested with the compiler
 */
#pragma warning compiler/clang/compilerSpecific.h is based on reading the manual, but hasnt been tested with the clang compiler

#ifdef __cplusplus

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */
#define CXX_PLACEMENT_DELETE
#define CXX_THROW_SPECIFICATION

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#if __has_attribute(format)
#   define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))
#else
#   define EPICS_PRINTF_STYLE
#endif

/*
 * Deprecation marker if possible
 */
#if __has_attribute(deprecated)
#   define EPICS_DEPRECATED __attribute__((deprecated))
#else
#   define EPICS_DEPRECATED
#endif

#endif  /* ifndef compilerSpecific_h */
