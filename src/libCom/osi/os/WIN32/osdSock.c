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
 *	    $Id$
 *
 *	    WIN32 specific initialisation for bsd sockets,
 *	    based on Chris Timossi's base/src/ca/windows_depend.c,
 *      and also further additions by Kay Kasemir when this was in
 *	    dllmain.cc
 *
 *      7-1-97  -joh-
 *
 */

/*
 * ANSI C
 */
#include <stdio.h>
#include <stdlib.h>

/*
 * WIN32 specific
 */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "errlog.h"
#include "epicsVersion.h"

static unsigned nAttached = 0;
static WSADATA WsaData; /* version of winsock */

epicsShareFunc unsigned epicsShareAPI wsaMajorVersion ()
{
	return (unsigned) LOBYTE( WsaData.wVersion );
}
	
/*
 * osiSockAttach()
 */
epicsShareFunc int epicsShareAPI osiSockAttach()
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

#	if defined ( _DEBUG ) && 0	  
		fprintf(stderr, "EPICS attached to winsock version %s\n", WsaData.szDescription);
#	endif	
	
	nAttached = 1u;

	return TRUE;
}

/*
 * osiSockRelease()
 */
epicsShareFunc void epicsShareAPI osiSockRelease()
{
	if (nAttached) {
		if (--nAttached==0u) {
			WSACleanup();
#			if defined ( _DEBUG ) && 0			  
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

