/* 
 * $Id$
 * 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#ifndef _WIN32
#error This source is specific to WIN32
#endif

#include <stdlib.h>

/*
 * Windows includes
 */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN 
#include <wtypes.h>
#include <process.h>

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
    (const char *pProcessName, const char *pBaseExecutableName)
{
	BOOL status;
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	GetStartupInfo (&startupInfo); 
	startupInfo.lpReserved = NULL;
	startupInfo.lpTitle = (char *) pProcessName;
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOWMINNOACTIVE;
	
	status =  CreateProcess( 
		            NULL, // pointer to name of executable module (not required if command line is specified)
		            (char *)pBaseExecutableName, // pointer to command line string 
		            NULL, // pointer to process security attributes 
		            NULL, // pointer to thread security attributes 
		            FALSE, // handle inheritance flag 
		            CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, // creation flags 
		            NULL, // pointer to new environment block (defaults to caller's environement)
		            NULL, // pointer to current directory name  (defaults to caller's current directory)
		            &startupInfo, // pointer to STARTUPINFO 
		            &processInfo // pointer to PROCESS_INFORMATION 
	); 
	if (status==0) {
		DWORD W32status;
		LPVOID errStrMsgBuf;
		LPVOID complteMsgBuf;

		W32status = FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			GetLastError (),
			MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &errStrMsgBuf,
			0,
			NULL 
		);

		if (W32status) {
			char *pFmtArgs[] = {
					"Failed to start executable -",
					(char *) pBaseExecutableName, 
					errStrMsgBuf,
					"Changes may be required in your \"path\" environment variable.",
					"PATH = ",
					getenv ("path")};

			if (pFmtArgs[5]==NULL) {
				pFmtArgs[5] = "<empty string>";
			}

			W32status = FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | 
					FORMAT_MESSAGE_ARGUMENT_ARRAY | 80,
				"%1 \"%2\". %3 %4 %5 \"%6\"",
				0,
				MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR) &complteMsgBuf,
				0,
				pFmtArgs 
			);
			if (W32status) {
				// Display the string.
				MessageBox (NULL, complteMsgBuf, "Configuration Problem", 
					MB_OK|MB_ICONINFORMATION);
				LocalFree (complteMsgBuf);
			}
			else {
				// Display the string.
				MessageBox (NULL, errStrMsgBuf, "Failed to start executable", 
					MB_OK|MB_ICONINFORMATION);
			}

			// Free the buffer.
			LocalFree (errStrMsgBuf);
		}
		else {
			errlogPrintf ("!!WARNING!!\n");
			errlogPrintf ("Unable to locate executable \"%s\".\n", pBaseExecutableName);
			errlogPrintf ("You may need to modify your environment.\n");
		}
	}

    return osiSpawnDetachedProcessSuccess;

	//
	// use of spawn here causes problems when the ca repeater
	// inherits open files (and sockets) from the spawning
	// process
	//
	//status = _spawnlp (_P_DETACH, pBaseExecutableName, pBaseExecutableName, NULL);
	//if (status<0) {
	//	errlogPrintf ("!!WARNING!!\n");
	//	errlogPrintf ("Unable to locate the EPICS executable \"%s\".\n",
	//		pBaseExecutableName);
	//	errlogPrintf ("You may need to modify your environment.\n");
	//}
}
