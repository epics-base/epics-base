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

typedef void *IMP_HANDLE;
int impInit(IMP_HANDLE *handle,int bufferSize);
void impFree(IMP_HANDLE handle);
int impSetIncludeToken(IMP_HANDLE handle,char *includeToken);
int impSetEnvName(IMP_HANDLE handle,char *envName);
MAC_HANDLE *impGetMacHandle(IMP_HANDLE handle);
int impSetMacHandle(IMP_HANDLE imp,MAC_HANDLE *mac);
void impMacFree(IMP_HANDLE imp);
int impMacAddSubstitutions(IMP_HANDLE handle,const char *substitutions);
int impMacAddNameValuePairs(IMP_HANDLE handle,const char *pairs[]);
int impSetPath(IMP_HANDLE handle,const char *path);
int impAddPath(IMP_HANDLE handle,const char *path);
int impPrintInclude(IMP_HANDLE handle,FILE *fp);
int impDumpPath(IMP_HANDLE handle);
int impOpenFile(IMP_HANDLE handle,char *filename);
int impCloseFile(IMP_HANDLE handle);
int impGetLine(IMP_HANDLE handle,char *buffer,int bufferSize);

/*status values*/
typedef enum{impSuccess,impFailure} impStatus;
extern char **impStatusMessage;

#endif /*INCimpLibh*/
