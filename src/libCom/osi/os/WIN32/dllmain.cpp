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

#include <windows.h>
#include <stdio.h>

#ifndef WIN32
#error This source is specific to WIN32 
#endif

extern int init_osi_time ();
extern int exit_osi_time ();

BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) 
	{
	case DLL_PROCESS_ATTACH:

#if _DEBUG
		/* for gui applications, setup console for error messages */
		if (AllocConsole())
		{
			SetConsoleTitle("Channel Access Status");
			freopen( "CONOUT$", "a", stderr );
		}
#endif	 		
		if (init_osi_time ())
			return FALSE;
		break;
	case DLL_PROCESS_DETACH:
		exit_osi_time ();
		break;
	}

	return TRUE;
}



