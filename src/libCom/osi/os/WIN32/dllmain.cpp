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
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *		Lawrence Berkley National Laboratory
 *
 *      Modification Log:
 *      -----------------
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

