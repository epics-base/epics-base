
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
 
#ifndef __BORLANDC__
#   error compiler/borland/compilerSpecific.h is only for use with the Borland compiler
#endif

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
#if __BORLANDC__ >= 0x600
#    define CXX_PLACEMENT_DELETE
#endif

#define CXX_THROW_SPECIFICATION

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#define EPICS_PRINTF_STYLE(f,a)

/*
 * Deprecation marker
 */
#define EPICS_DEPRECATED

#endif  /* ifndef compilerSpecific_h */
