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
#include "compilerDependencies.h"

#ifdef  __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI epicsSnprintf(
    char *str, size_t size, const char *format, ...) EPICS_PRINTF_STYLE(3,4);
epicsShareFunc int epicsShareAPI epicsVsnprintf(
    char *str, size_t size, const char *format, va_list ap);
epicsShareFunc void epicsShareAPI epicsTempName ( 
    char * pNameBuf, size_t nameBufLength );
epicsShareFunc FILE * epicsShareAPI epicsTempFile ();

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
epicsShareFunc enum TF_RETURN epicsShareAPI truncateFile ( const char *pFileName, unsigned size );

/*The followig are for redirecting stdin,stdout,stderr */
epicsShareFunc FILE * epicsShareAPI epicsGetStdin(void);
epicsShareFunc FILE * epicsShareAPI epicsGetStdout(void);
epicsShareFunc FILE * epicsShareAPI epicsGetStderr(void);
epicsShareFunc void  epicsShareAPI epicsSetStdin(FILE *);
epicsShareFunc void  epicsShareAPI epicsSetStdout(FILE *);
epicsShareFunc void  epicsShareAPI epicsSetStderr(FILE *);

epicsShareFunc int epicsShareAPI epicsStdoutPrintf(
    const char *pformat, ...) EPICS_PRINTF_STYLE(1,2);

#ifndef epicsStdioPVT
#undef stdin
#define stdin epicsGetStdin()
#undef stdout
#define stdout epicsGetStdout()
#undef stderr
#define stderr epicsGetStderr()

/*The following are for making printf be fprintf(stdout */
#ifdef printf
#undef printf
#endif /*printf*/
#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
#define printf(...) epicsStdoutPrintf(...) 
#elif defined(__GNUC__)
#define printf(format...) epicsStdoutPrintf(format)
#else
#define printf epicsStdoutPrintf
#endif /* defined(__STDC_VERSION__) && __STDC_VERSION__>=199901_*/
#endif /* epicsStdioPVT */


#ifdef  __cplusplus
}
#endif

#endif /* epicsStdioh */
