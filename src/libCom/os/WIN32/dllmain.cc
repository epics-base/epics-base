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

#include <winsock2.h>
#include <stdio.h>

#include "epicsVersion.h"
#define epicsExportSharedSymbols
#include "bsdSocketResource.h"

#ifndef _WIN32
#error This source is specific to WIN32 
#endif

extern int init_osi_time ();
extern int exit_osi_time ();

#if !defined(EPICS_DLL_NO)
BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) 
	{
	case DLL_PROCESS_ATTACH:
		if (!bsdSockAttach()) 
			return FALSE;
		if (init_osi_time ())
			return FALSE;
#		ifdef _DEBUG
			fprintf(stderr, "Process attached to Com.dll version %s\n", EPICS_VERSION_STRING);
#		endif
		break;

	case DLL_PROCESS_DETACH:
		bsdSockRelease();
		exit_osi_time ();
#		ifdef _DEBUG
			fprintf(stderr, "Process detached from Com.dll version %s\n", EPICS_VERSION_STRING);
#		endif
		break;
	}

	return TRUE;
}
#endif

