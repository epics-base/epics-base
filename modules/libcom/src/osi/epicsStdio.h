/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* epicsStdio.h */

#ifndef epicsStdioh
#define epicsStdioh

#include <stdio.h>
#include <stdarg.h>

#include "shareLib.h"
#include "compilerDependencies.h"
#include "epicsTempFile.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef epicsStdioStdStreams
#  undef stdin
#  define stdin epicsGetStdin()
#  undef stdout
#  define stdout epicsGetStdout()
#  undef stderr
#  define stderr epicsGetStderr()
#endif

/* Make printf, puts and putchar use *our* version of stdout */

#ifdef printf
#  undef printf
#endif /* printf */
#define printf epicsStdoutPrintf

#ifdef puts
#  undef puts
#endif /* puts */
#define puts epicsStdoutPuts

#ifdef putchar
#  undef putchar
#endif /* putchar */
#define putchar epicsStdoutPutchar

epicsShareFunc int epicsShareAPI epicsSnprintf(
    char *str, size_t size, const char *format, ...) EPICS_PRINTF_STYLE(3,4);
epicsShareFunc int epicsShareAPI epicsVsnprintf(
    char *str, size_t size, const char *format, va_list ap);

/*
 * truncate to specified size (we dont use truncate()
 * because it is not portable)
 *
 * pFileName - name (and optionally path) of file
 * size - the new file size (if file is curretly larger)
 * 
 * returns TF_OK if the file is less than size bytes
 * or if it was successfully truncated. Returns
 * TF_ERROR if the file could not be truncated.
 */
enum TF_RETURN {TF_OK=0, TF_ERROR=1};
epicsShareFunc enum TF_RETURN truncateFile ( const char *pFileName, unsigned long size );

/* The following are for redirecting stdin,stdout,stderr */
epicsShareFunc FILE * epicsShareAPI epicsGetStdin(void);
epicsShareFunc FILE * epicsShareAPI epicsGetStdout(void);
epicsShareFunc FILE * epicsShareAPI epicsGetStderr(void);
/* These are intended for iocsh only */
epicsShareFunc FILE * epicsShareAPI epicsGetThreadStdin(void);
epicsShareFunc FILE * epicsShareAPI epicsGetThreadStdout(void);
epicsShareFunc FILE * epicsShareAPI epicsGetThreadStderr(void);
epicsShareFunc void  epicsShareAPI epicsSetThreadStdin(FILE *);
epicsShareFunc void  epicsShareAPI epicsSetThreadStdout(FILE *);
epicsShareFunc void  epicsShareAPI epicsSetThreadStderr(FILE *);

epicsShareFunc int epicsShareAPI epicsStdoutPrintf(
    const char *pformat, ...) EPICS_PRINTF_STYLE(1,2);
epicsShareFunc int epicsShareAPI epicsStdoutPuts(const char *str);
epicsShareFunc int epicsShareAPI epicsStdoutPutchar(int c);

#ifdef  __cplusplus
}
#endif

#endif /* epicsStdioh */
