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

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#define epicsPrintUseProtoANSI
#endif

#ifdef __STDC__
#ifndef epicsPrintUseProtoANSI
#define epicsPrintUseProtoANSI
#endif
#endif


/* define errMessage with a macro so we can print the file and line number*/
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, PM)
/* epicsPrintf and epicsVprintf old versions of errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

typedef void(*errlogListener) (const char *message);
typedef enum {errlogInfo,errlogMinor,errlogMajor,errlogFatal} errlogSevEnum;
extern char * errlogSevEnumString[];
#ifdef ERRLOG_INIT
char * errlogSevEnumString[] = {"info","minor","major","fatal"};
#endif

#ifdef epicsPrintUseProtoANSI

epicsShareFunc int epicsShareAPI errlogPrintf(
    const char *pformat, ...);
epicsShareFunc int epicsShareAPI errlogVprintf(
    const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPI errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...);
epicsShareFunc int epicsShareAPI errlogSevVprintf(
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

epicsShareFunc void epicsShareAPI eltc(int yesno);
epicsShareFunc void epicsShareAPI errlogInit(int bufsize);

/*other routines that write to log file*/
epicsShareFunc void epicsShareAPI errPrintf(long status, const char *pFileName,
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
