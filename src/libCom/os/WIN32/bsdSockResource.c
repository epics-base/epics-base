
/*
 *	bsdSockResource.c
 *
 *	WIN32 specific initialisation for bsd sockets,
 *	based on Chris Timossi's base/src/ca/windows_depend.c,
 *  and also further additions by Kay Kasemir when this was in
 *	dllmain.cc
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

#include <stdio.h>

#include "epicsVersion.h"
#define epicsExportSharedSymbols
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
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
epicsShareFunc void epicsShareAPI ipAddrToA 
			(const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize)
{
	char			*pString;
	struct hostent	*ent;
#	define			maxPortDigits 5u

	if (bufSize<1) {
		return;
	}

	if (paddr->sin_family!=AF_INET) {
		strncpy(pBuf, "<Ukn Addr Type>", bufSize-1);
		/*
		 * force null termination
		 */
		pBuf[bufSize-1] = '\0';
	}
	else {
		ent = gethostbyaddr((char *) &paddr->sin_addr, 
				sizeof(paddr->sin_addr), AF_INET);
		if(ent){
			pString = ent->h_name;
		}
		else{
			pString = inet_ntoa (paddr->sin_addr);
		}

		/*
		 * allow space for the port number
		 */
		if (bufSize>maxPortDigits+strlen(pString)) {
			sprintf (pBuf, "%.*s:%u", bufSize-maxPortDigits-1, 
				pString, ntohs(paddr->sin_port));
		}
		else {
			sprintf (pBuf, "%.*s", bufSize-1, pString);
		}
	}
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

