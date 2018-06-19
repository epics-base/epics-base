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

#include "shareLib.h"
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

epicsShareExtern int errVerbose;


#ifdef ERRLOG_INIT
    epicsShareDef const char *errlogSevEnumString[] = {
        "info",
        "minor",
        "major",
        "fatal"
    };
#else
    epicsShareExtern const char * errlogSevEnumString[];
#endif

/* errMessage is a macro so it can get the file and line number */
#define errMessage(S, PM) \
     errPrintf(S, __FILE__, __LINE__, "%s", PM)
/* epicsPrintf and epicsVprintf are old names for errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

epicsShareFunc int errlogPrintf(const char *pformat, ...)
    EPICS_PRINTF_STYLE(1,2);
epicsShareFunc int errlogVprintf(const char *pformat, va_list pvar);
epicsShareFunc int errlogSevPrintf(const errlogSevEnum severity,
    const char *pformat, ...) EPICS_PRINTF_STYLE(2,3);
epicsShareFunc int errlogSevVprintf(const errlogSevEnum severity,
    const char *pformat, va_list pvar);
epicsShareFunc int errlogMessage(const char *message);

epicsShareFunc const char * errlogGetSevEnumString(errlogSevEnum severity);
epicsShareFunc void errlogSetSevToLog(errlogSevEnum severity);
epicsShareFunc errlogSevEnum errlogGetSevToLog(void);

epicsShareFunc void errlogAddListener(errlogListener listener, void *pPrivate);
epicsShareFunc int errlogRemoveListeners(errlogListener listener,
    void *pPrivate);

epicsShareFunc int eltc(int yesno);
epicsShareFunc int errlogSetConsole(FILE *stream);

epicsShareFunc int errlogInit(int bufsize);
epicsShareFunc int errlogInit2(int bufsize, int maxMsgSize);
epicsShareFunc void errlogFlush(void);

epicsShareFunc void errPrintf(long status, const char *pFileName, int lineno,
    const char *pformat, ...) EPICS_PRINTF_STYLE(4,5);

epicsShareFunc int errlogPrintfNoConsole(const char *pformat, ...)
    EPICS_PRINTF_STYLE(1,2);
epicsShareFunc int errlogVprintfNoConsole(const char *pformat,va_list pvar);

epicsShareFunc void errSymLookup(long status, char *pBuf, size_t bufLength);

#ifdef __cplusplus
}
#endif

#endif /*INC_errlog_H*/
