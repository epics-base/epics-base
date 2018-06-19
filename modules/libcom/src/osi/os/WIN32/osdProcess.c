/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#ifndef _WIN32
#error This source is specific to WIN32
#endif

#include <stdlib.h>

#define STRICT
#include <windows.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"
#include "errlog.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
    DWORD bufsize;

    if ( bufSizeIn > 0xffffffff ) {
        return osiGetUserNameFail;
    }

    bufsize = (DWORD) bufSizeIn;

    if ( ! GetUserName (pBuf, &bufsize) ) {
        return osiGetUserNameFail;
    }

    if ( *pBuf == '\0' ) {
        return osiGetUserNameFail;
    }

    return osiGetUserNameSuccess;
}

epicsShareFunc osiSpawnDetachedProcessReturn epicsShareAPI osiSpawnDetachedProcess 
    ( const char *pProcessName, const char *pBaseExecutableName )
{
	BOOL status;
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	GetStartupInfo ( &startupInfo ); 
	startupInfo.lpReserved = NULL;
	startupInfo.lpTitle = (char *) pProcessName;
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOWMINNOACTIVE;
	
	status =  CreateProcess ( 
        NULL, /* pointer to name of executable module (not required if command line is specified) */
        (char *) pBaseExecutableName, /* pointer to command line string */
        NULL, /* pointer to process security attributes */
        NULL, /* pointer to thread security attributes */
        FALSE, /* handle inheritance flag */
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, /* creation flags */
        NULL, /* pointer to new environment block (defaults to caller's environement) */
        NULL, /* pointer to current directory name  (defaults to caller's current directory) */
        &startupInfo, /* pointer to STARTUPINFO */
        &processInfo /* pointer to PROCESS_INFORMATION */
    ); 
    if ( status == 0 ) {
        DWORD W32status;
        LPVOID errStrMsgBuf;
        LPVOID complteMsgBuf;
        
        W32status = FormatMessage ( 
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError (),
            MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
            	(LPTSTR) &errStrMsgBuf,
            0,
            NULL 
        );

        if ( W32status ) {
            char *pFmtArgs[6];
            pFmtArgs[0] = "Failed to start executable -";
            pFmtArgs[1] = (char *) pBaseExecutableName;
            pFmtArgs[2] = errStrMsgBuf;
            pFmtArgs[3] = "Changes may be required in your \"path\" environment variable.";
            pFmtArgs[4] = "PATH = ";
            pFmtArgs[5] = getenv ("path");
            if ( pFmtArgs[5] == NULL ) {
                pFmtArgs[5] = "<empty string>";
            }
            
            W32status = FormatMessage( 
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | 
                    FORMAT_MESSAGE_ARGUMENT_ARRAY | 80,
                "%1 \"%2\". %3 %4 %5 \"%6\"",
                0,
                MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
                (LPTSTR) &complteMsgBuf,
                0,
                pFmtArgs 
            );
            if (W32status) {
                /* Display the string. */
                MessageBox (NULL, complteMsgBuf, "Configuration Problem", 
                	MB_OK | MB_ICONINFORMATION);
                LocalFree (complteMsgBuf);
            }
            else {
                /* Display the string. */
                MessageBox (NULL, errStrMsgBuf, "Failed to start executable", 
                    MB_OK | MB_ICONINFORMATION);
            }
            
            /* Free the buffer. */
            LocalFree (errStrMsgBuf);
        }
        else {
            errlogPrintf ("!!WARNING!!\n");
            errlogPrintf ("Unable to locate executable \"%s\".\n", pBaseExecutableName);
            errlogPrintf ("You may need to modify your \"path\" environment variable.\n");
        }
        return osiSpawnDetachedProcessFail;
    }

    return osiSpawnDetachedProcessSuccess;
}
