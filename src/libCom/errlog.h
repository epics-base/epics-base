/* src/libCom/errlog.h */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCerrlogh
#define INCerrlogh



#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#define epicsPrintUseProtoANSI
#endif

#ifdef __STDC__
#ifndef epicsPrintUseProtoANSI
#define epicsPrintUseProtoANSI
#endif
#endif

#ifdef epicsPrintUseProtoANSI
#	include <stdarg.h>
#else
#	include <varargs.h>
#endif

/* define errMessage with a macro so we can print the file and line number*/
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, PM)
/* epicsPrintf and epicsVprintf old versions of errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

#ifdef __STDC__
typedef void(*errlogListener) (const char *message);
#else
typedef void(*errlogListener) ();
#endif
typedef enum {errlogInfo,errlogMinor,errlogMajor,errlogFatal} errlogSevEnum;

#ifdef ERRLOG_INIT
epicsShareDef char * errlogSevEnumString[] = {"info","minor","major","fatal"};
#else
epicsShareExtern char * errlogSevEnumString[];
#endif

#ifdef epicsPrintUseProtoANSI

epicsShareFunc int epicsShareAPIV errlogPrintf(
    const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogVprintf(
    const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPIV errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogSevVprintf(
    const errlogSevEnum severity,const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPI errlogMessage(
	const char *message);

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity);
epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity );
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog(void);

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener);
epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener);

epicsShareFunc int epicsShareAPI eltc(int yesno);
epicsShareFunc int epicsShareAPI errlogInit(int bufsize);

/*other routines that write to log file*/
epicsShareFunc void epicsShareAPIV errPrintf(long status, const char *pFileName,
    int lineno, const char *pformat, ...);

#else /* not epicsPrintUseProtoANSI */
epicsShareFunc int epicsShareAPI errlogPrintf();
epicsShareFunc int epicsShareAPI errlogVprintf();
epicsShareFunc int epicsShareAPI errlogSevPrintf();
epicsShareFunc int epicsShareAPI errlogSevVprintf();
epicsShareFunc int epicsShareAPI errlogMessage();
epicsShareFunc char * epicsShareAPI errlogGetSevEnumString();
epicsShareFunc void epicsShareAPI errlogSetSevToLog();
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog();
epicsShareFunc void epicsShareAPI errlogAddListener();
epicsShareFunc void epicsShareAPI errlogRemoveListener();
epicsShareFunc void epicsShareAPI eltc();
epicsShareFunc void epicsShareAPI errlogInit();
epicsShareFunc void epicsShareAPI errPrintf();
#endif /* ifdef epicsPrintUseProtoANSI */

#ifdef __cplusplus
}
#endif

#endif /*INCerrlogh*/
