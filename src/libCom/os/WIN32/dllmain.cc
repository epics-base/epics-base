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

#include "epicsVersion.h"

#ifndef _WIN32
#error This source is specific to WIN32 
#endif

extern int init_osi_time ();
extern int exit_osi_time ();

BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	WSADATA WsaData;
	switch (dwReason) 
	{
	case DLL_PROCESS_ATTACH:

#if _DEBUG
		/* for gui applications, setup console for error messages */
		if (AllocConsole())
		{
			SetConsoleTitle(BASE_VERSION_STRING);
			freopen( "CONOUT$", "a", stderr );
			fprintf(stderr, "Process attached to Com.dll version %s\n", EPICS_VERSION_STRING);
		}
#endif	 		
		if (init_osi_time ())
			return FALSE;

		/*
		 * for use of select() by the fd managers
		 */
		if (WSAStartup(MAKEWORD(/*major*/2,/*minor*/2), &WsaData) != 0) {
			/*
			 * The winsock I & II doc indicate that these steps are not required,
			 * but experience at some sites proves otherwise. Perhaps some vendors 
			 * do not follow the protocol described in the doc.
			 */
			if (WSAStartup(MAKEWORD(/*major*/1,/*minor*/1), &WsaData) != 0) {
				if (WSAStartup(MAKEWORD(/*major*/1,/*minor*/0), &WsaData) != 0) {
					WSACleanup();
					fprintf(stderr,"Unable to attach to winsock version 2.2 or lower\n");
					return FALSE;
				}
			}
		}

#if _DEBUG			  
		fprintf(stderr, "EPICS Com.dll attached to winsock version %s\n", WsaData.szDescription);
#endif	
		break;
	case DLL_PROCESS_DETACH:
		exit_osi_time ();
		if (WSACleanup()!=0)
			return FALSE;
		break;
	}

	return TRUE;
}



