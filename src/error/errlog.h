/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INCerrlogh
#define INCerrlogh

#include <stdarg.h>
#include <stdio.h>

#include "shareLib.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif


/* define errMessage with a macro so we can print the file and line number*/
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, "%s", PM)
/* epicsPrintf and epicsVprintf old versions of errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

typedef void (*errlogListener)(void *pPrivate, const char *message);

typedef enum {errlogInfo, errlogMinor, errlogMajor, errlogFatal} errlogSevEnum;

#ifdef ERRLOG_INIT
epicsShareDef char * errlogSevEnumString[] = {"info","minor","major","fatal"};
#else
epicsShareExtern char * errlogSevEnumString[];
#endif

epicsShareFunc int errlogPrintf(
    const char *pformat, ...) EPICS_PRINTF_STYLE(1,2);
epicsShareFunc int errlogVprintf(
    const char *pformat,va_list pvar);
epicsShareFunc int errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...) EPICS_PRINTF_STYLE(2,3);
epicsShareFunc int errlogSevVprintf(
    const errlogSevEnum severity,const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPI errlogMessage(
    const char *message);

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity);
epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity );
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog(void);

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener, void *pPrivate);
epicsShareFunc int epicsShareAPI errlogRemoveListeners(
    errlogListener listener, void *pPrivate);

epicsShareFunc int epicsShareAPI eltc(int yesno);
epicsShareFunc int errlogSetConsole(FILE *stream);

epicsShareFunc int epicsShareAPI errlogInit(int bufsize);
epicsShareFunc int epicsShareAPI errlogInit2(int bufsize, int maxMsgSize);
epicsShareFunc void epicsShareAPI errlogFlush(void);

/*other routines that write to log file*/
epicsShareFunc void errPrintf(long status, const char *pFileName,
    int lineno, const char *pformat, ...) EPICS_PRINTF_STYLE(4,5);

epicsShareExtern int errVerbose;

/* The following are added so that logMsg on vxWorks does not cause
 * the message to appear twice on the console
 */
epicsShareFunc int errlogPrintfNoConsole(
    const char *pformat, ...) EPICS_PRINTF_STYLE(1,2);
epicsShareFunc int errlogVprintfNoConsole(
    const char *pformat,va_list pvar);

#ifdef __cplusplus
}
#endif

#endif /*INCerrlogh*/
