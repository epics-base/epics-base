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
 *	dllmain.c
 *
 *	WIN32 specific initialisation for the Com library,
 *	based on Chris Timossi's base/src/ca/windows_depend.c,
 *	especially initializing:
 *		ositime
 *
 *      8-2-96  -kuk-
 *
 */

#include <stdio.h>
#include <stdlib.h>

#define VC_EXTRALEAN
#define STRICT
#include <winsock2.h>

#include "epicsVersion.h"
#define epicsExportSharedSymbols
#include "osiSock.h"

#ifndef _WIN32
#   error This source is specific to WIN32 
#endif

extern "C" void epicsThreadCleanupWIN32 ();

#if !defined(EPICS_DLL_NO)
BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) 
	{
	case DLL_PROCESS_ATTACH:
		//if ( ! osiSockAttach() ) 
		//	return FALSE;
#		if defined ( _DEBUG ) && 0
			fprintf(stderr, "Process attached to Com.dll version %s\n", EPICS_VERSION_STRING);
#		endif
		break;

	case DLL_PROCESS_DETACH:
		//osiSockRelease();
        //epicsThreadCleanupWIN32 ();
#		if defined ( _DEBUG ) && 0
			fprintf(stderr, "Process detached from Com.dll version %s\n", EPICS_VERSION_STRING);
#		endif
		break;

	case DLL_THREAD_DETACH:
        //epicsThreadCleanupWIN32 ();
        break;
	}

	return TRUE;
}
#endif

