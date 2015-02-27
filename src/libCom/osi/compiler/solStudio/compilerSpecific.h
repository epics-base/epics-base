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

#if !defined(__SUNPRO_C) && !defined (__SUNPRO_CC)
#  error Not Solaris Studio
#endif

#if (defined(__SUNPRO_C) && __SUNPRO_C < 0x590) || \
    (defined(__SUNPRO_CC) && __SUNPRO_CC < 0x590)
#  define EPICS_ALWAYS_INLINE inline
#else
#  define EPICS_ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#ifdef __cplusplus

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 *
 * (our default guess is that the compiler implements the C++ 97 standard)
 */
#define CXX_THROW_SPECIFICATION
#define CXX_PLACEMENT_DELETE

#endif /* __cplusplus */


#endif  /* ifndef compilerSpecific_h */
