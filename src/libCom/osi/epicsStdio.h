/* epicsStdio.h */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef epicsStdioh
#define epicsStdioh

#include <stdio.h>
#include <stdarg.h>

#include "shareLib.h"
#include "cxxCompilerDependencies.h"

#ifdef  __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI epicsSnprintf(
    char *str, size_t size, const char *format, ...) EPICS_PRINTF_STYLE(3,4);
epicsShareFunc int epicsShareAPI epicsVsnprintf(
    char *str, size_t size, const char *format, va_list ap);
epicsShareFunc FILE * epicsShareAPI epicsTempFile ( void );

#ifdef  __cplusplus
}
#endif

#endif /* epicsStdioh */
