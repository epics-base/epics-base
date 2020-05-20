/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_errlog_H
#define INC_errlog_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "libComAPI.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*errlogListener)(void *pPrivate, const char *message);

typedef enum {
    errlogInfo,
    errlogMinor,
    errlogMajor,
    errlogFatal
} errlogSevEnum;

LIBCOM_API extern int errVerbose;


#ifdef ERRLOG_INIT
    const char *errlogSevEnumString[] = {
        "info",
        "minor",
        "major",
        "fatal"
    };
#else
    LIBCOM_API extern const char * errlogSevEnumString[];
#endif

/* errMessage is a macro so it can get the file and line number */
#define errMessage(S, PM) \
     errPrintf(S, __FILE__, __LINE__, "%s", PM)
/* epicsPrintf and epicsVprintf are old names for errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

LIBCOM_API int errlogPrintf(const char *pformat, ...)
    EPICS_PRINTF_STYLE(1,2);
LIBCOM_API int errlogVprintf(const char *pformat, va_list pvar);
LIBCOM_API int errlogSevPrintf(const errlogSevEnum severity,
    const char *pformat, ...) EPICS_PRINTF_STYLE(2,3);
LIBCOM_API int errlogSevVprintf(const errlogSevEnum severity,
    const char *pformat, va_list pvar);
LIBCOM_API int errlogMessage(const char *message);

LIBCOM_API const char * errlogGetSevEnumString(errlogSevEnum severity);
LIBCOM_API void errlogSetSevToLog(errlogSevEnum severity);
LIBCOM_API errlogSevEnum errlogGetSevToLog(void);

LIBCOM_API void errlogAddListener(errlogListener listener, void *pPrivate);
LIBCOM_API int errlogRemoveListeners(errlogListener listener,
    void *pPrivate);

LIBCOM_API int eltc(int yesno);
LIBCOM_API int errlogSetConsole(FILE *stream);

LIBCOM_API int errlogInit(int bufsize);
LIBCOM_API int errlogInit2(int bufsize, int maxMsgSize);
LIBCOM_API void errlogFlush(void);

LIBCOM_API void errPrintf(long status, const char *pFileName, int lineno,
    const char *pformat, ...) EPICS_PRINTF_STYLE(4,5);

LIBCOM_API int errlogPrintfNoConsole(const char *pformat, ...)
    EPICS_PRINTF_STYLE(1,2);
LIBCOM_API int errlogVprintfNoConsole(const char *pformat,va_list pvar);

LIBCOM_API void errSymLookup(long status, char *pBuf, size_t bufLength);

#ifdef __cplusplus
}
#endif

#endif /*INC_errlog_H*/
