/*impLib.h*/
/*Processes include, macro substitution, and path*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
 
/* Modification Log:
 * -----------------
 * 01  17sep96	mrk	Original
 */

#ifndef INCimpLibh
#define INCimpLibh

#include "macLib.h"
#include "shareLib.h"

typedef void *IMP_HANDLE;
epicsShareFunc int epicsShareAPI 
	impInit(IMP_HANDLE *handle,int bufferSize);
epicsShareFunc void epicsShareAPI 
	impFree(IMP_HANDLE handle);
epicsShareFunc int epicsShareAPI 
	impSetIncludeToken(IMP_HANDLE handle,char *includeToken);
epicsShareFunc int epicsShareAPI 
	impSetEnvName(IMP_HANDLE handle,char *envName);
epicsShareFunc MAC_HANDLE * 
	epicsShareAPI impGetMacHandle(IMP_HANDLE handle);
epicsShareFunc int epicsShareAPI 
	impSetMacHandle(IMP_HANDLE imp,MAC_HANDLE *mac);
epicsShareFunc void epicsShareAPI 
	impMacFree(IMP_HANDLE imp);
epicsShareFunc int epicsShareAPI 
	impMacAddSubstitutions(IMP_HANDLE handle,const char *substitutions);
epicsShareFunc int epicsShareAPI 
	impMacAddNameValuePairs(IMP_HANDLE handle,const char *pairs[]);
epicsShareFunc int epicsShareAPI 
	impSetPath(IMP_HANDLE handle,const char *path);
epicsShareFunc int epicsShareAPI 
	impAddPath(IMP_HANDLE handle,const char *path);
epicsShareFunc int epicsShareAPI 
	impPrintInclude(IMP_HANDLE handle,FILE *fp);
epicsShareFunc int epicsShareAPI 
	impDumpPath(IMP_HANDLE handle);
epicsShareFunc int epicsShareAPI 
	impOpenFile(IMP_HANDLE handle,char *filename);
epicsShareFunc int epicsShareAPI 
	impCloseFile(IMP_HANDLE handle);
epicsShareFunc int epicsShareAPI 
	impGetLine(IMP_HANDLE handle,char *buffer,int bufferSize);

/*status values*/
typedef enum{impSuccess,impFailure} impStatus;
extern char **impStatusMessage;

#endif /*INCimpLibh*/
