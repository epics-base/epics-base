/*************************************************************************\
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef _EPICS_COMPILERDEFS_H
#define _EPICS_COMPILERDEFS_H

/*
 * Compiler-specific definitions
 */

#ifdef __GNUC__
# define EPICS_PRINTF_STYLE(f,a) __attribute__((format(printf,f,a)))
#else
# define EPICS_PRINTF_STYLE(f,a)
#endif

#endif /* _EPICS_COMPILERDEFS_H */
