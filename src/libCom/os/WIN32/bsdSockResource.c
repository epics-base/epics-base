
/*
 *	    $Id$
 *
 *	    WIN32 specific initialisation for bsd sockets,
 *	    based on Chris Timossi's base/src/ca/windows_depend.c,
 *      and also further additions by Kay Kasemir when this was in
 *	    dllmain.cc
 *
 *      7-1-97  -joh-
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

#ifndef _WIN32
#error This source is specific to WIN32 
#endif

/*
 * ANSI C
 */
#include <stdio.h>

/*
 * WIN32 specific
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#define epicsExportSharedSymbols
#include "epicsVersion.h"
#include "bsdSocketResource.h"

static unsigned nAttached = 0;
static WSADATA WsaData; /* version of winsock */

epicsShareFunc unsigned epicsShareAPI wsaMajorVersion()
{
	return (unsigned) LOBYTE( WsaData.wVersion );
}
	
/*
 * bsdSockAttach()
 */
epicsShareFunc int epicsShareAPI bsdSockAttach()
{
	int status;

	if (nAttached) {
		nAttached++;
		return TRUE;
	}

#if _DEBUG
	/* for gui applications, setup console for error messages */
	if (AllocConsole())
	{
		char title[256];
		DWORD titleLength = GetConsoleTitle(title, sizeof(title));
		if (titleLength) {
			titleLength = strlen (title);
			strncat (title, " " BASE_VERSION_STRING, sizeof(title));
		}
		else {
			strncpy(title, BASE_VERSION_STRING, sizeof(title));	
		}
		title[sizeof(title)-1]= '\0';
		SetConsoleTitle(title);
		freopen( "CONOUT$", "a", stderr );
	}
#endif

	/*
	 * attach to win sock
	 */
	status = WSAStartup(MAKEWORD(/*major*/2,/*minor*/2), &WsaData);
	if (status != 0) {
		fprintf(stderr,
			"Unable to attach to windows sockets version 2. error=%d\n", status);
		fprintf(stderr,
			"A Windows Sockets II update for windows 95 is available at\n");
		fprintf(stderr,
			"http://www.microsoft.com/windows95/info/ws2.htm");
		return FALSE;
	}

#	if _DEBUG			  
		fprintf(stderr, "EPICS attached to winsock version %s\n", WsaData.szDescription);
#	endif	
	
	nAttached = 1u;

	return TRUE;
}

/*
 * bsdSockRelease()
 */
epicsShareFunc void epicsShareAPI bsdSockRelease()
{
	if (nAttached) {
		if (--nAttached==0u) {
			WSACleanup();
#			if _DEBUG			  
				fprintf(stderr, "EPICS released winsock version %s\n", WsaData.szDescription);
#			endif
			memset (&WsaData, '\0', sizeof(WsaData));
		}
	}
}

/*
 * ipAddrToHostName
 */
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize)
{
	struct hostent	*ent;

	if (bufSize<1) {
		return 0;
	}

	ent = gethostbyaddr((char *) pAddr, sizeof (*pAddr), AF_INET);
	if (ent) {
        strncpy (pBuf, ent->h_name, bufSize);
        pBuf[bufSize-1] = '\0';
        return strlen (pBuf);
	}
    return 0;
}

/*
 * hostToIPAddr ()
 */
epicsShareFunc int epicsShareAPI hostToIPAddr 
				(const char *pHostName, struct in_addr *pIPA)
{
	struct hostent *phe;

	phe = gethostbyname (pHostName);
	if (phe && phe->h_addr_list[0]) {
		if (phe->h_addrtype==AF_INET && phe->h_length<=sizeof(struct in_addr)) {
			struct in_addr *pInAddrIn = (struct in_addr *) phe->h_addr_list[0];
			
			*pIPA = *pInAddrIn;

			/*
			 * success
			 */
			return 0;
		}
	}

	/*
	 * return indicating an error
	 */
	return -1;
}

/*
 * convertSocketErrorToString()
 */
epicsShareFunc const char * epicsShareAPI convertSocketErrorToString (int errnoIn)
{
	static char errString[128];
	static int init = 0;

	/*
	 * unfortunately, this does not work ...
	 * and there is no obvious replacement ...
	 */
#if 0
	DWORD W32status;

	W32status = FormatMessage( 
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		errnoIn,
		MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
		errString,
		sizeof(errString)/sizeof(errString[0]),
		NULL 
	);

	if (W32status==0) {
		sprintf (errString, "WIN32 Socket Library Error %d", errnoIn);
	}
	return errString;
#else
	sprintf (errString, "WIN32 Socket Library Error %d", errnoIn);
	return errString;
#endif
}

