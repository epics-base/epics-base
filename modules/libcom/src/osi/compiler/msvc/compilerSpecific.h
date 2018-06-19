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
 
#ifndef _MSC_VER
#   error compiler/msvc/compilerSpecific.h is only for use with the Microsoft compiler
#endif

#if _MSC_VER >= 1200
#define EPICS_ALWAYS_INLINE __forceinline
#else
#define EPICS_ALWAYS_INLINE __inline
#endif

/* Expands to a 'const char*' which describes the name of the current function scope */
#define EPICS_FUNCTION __FUNCTION__

#ifdef __cplusplus

/*
 * in general we dont like ifdefs but they do allow us to check the
 * compiler version and make the optimistic assumption that 
 * standards incompliance issues will be fixed by future compiler 
 * releases
 */
 
/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */
#if _MSC_VER >= 1200  /* visual studio 6.0 or later */
#    define CXX_PLACEMENT_DELETE
#endif

#if _MSC_VER > 1300  /* some release after visual studio 7 we hope */
#    define CXX_THROW_SPECIFICATION
#endif

#endif /* __cplusplus */


#endif  /* ifndef compilerSpecific_h */
