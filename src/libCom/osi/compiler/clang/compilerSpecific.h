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

#if __has_attribute(always_inline)
#define EPICS_ALWAYS_INLINE __inline__ __attribute__((always_inline))
#else
#define EPICS_ALWAYS_INLINE __inline__
#endif

/* Expands to a 'const char*' which describes the name of the current function scope */
#define EPICS_FUNCTION __PRETTY_FUNCTION__

#ifdef __cplusplus

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */
#define CXX_PLACEMENT_DELETE
#define CXX_THROW_SPECIFICATION

#endif /* __cplusplus */

/*
 * __has_attribute() is not supported on all versions of clang yet
 */

/*
 * Enable format-string checking
 */
#define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))

/*
 * Deprecation marker
 */
#define EPICS_DEPRECATED __attribute__((deprecated))

/*
 * Unused marker
 */
#define EPICS_UNUSED __attribute__((unused))

#endif  /* ifndef compilerSpecific_h */
