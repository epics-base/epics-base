
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

/*
 * osiLocalAddr ()
 */
epicsShareFunc osiSockAddr epicsShareAPI osiLocalAddr (SOCKET socket)
{
	static osiSockAddr  addr;
	static char     	init;
	int             	status;
	INTERFACE_INFO		*pIfinfo;
	INTERFACE_INFO      *pIfinfoList;
	unsigned			nelem;
	DWORD				numifs;
	DWORD				cbBytesReturned;

	if (init) {
		return addr;
	}

    init = 1;
    addr.sa.sa_family = AF_UNSPEC;

	/* only valid for winsock 2 and above */
	if (wsaMajorVersion() < 2 ) {
		return addr;
	}

	nelem = 10;
	pIfinfoList = (INTERFACE_INFO *) calloc ( nelem, sizeof (INTERFACE_INFO) );
	if (!pIfinfoList) {
		errlogPrintf ("calloc failed\n");
		return addr;
    }

	status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST, NULL, 0,
						(LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
						&cbBytesReturned, NULL, NULL);

	if (status != 0 || cbBytesReturned == 0) {
		errlogPrintf ("WSAIoctl SIO_GET_INTERFACE_LIST failed %d\n",WSAGetLastError());
		free (pIfinfoList);		
		return addr;
    }

	numifs = cbBytesReturned / sizeof(INTERFACE_INFO);
	for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

		/*
		 * dont use interfaces that have been disabled
		 */
		if (!(pIfinfo->iiFlags & IFF_UP)) {
			continue;
		}

		/*
		 * dont use the loop back interface
		 */
		if (pIfinfo->iiFlags & IFF_LOOPBACK) {
			continue;
		}

		addr.sa = pIfinfo->iiAddress.Address;

		/* Work around MS Winsock2 bugs */
        if (addr.sa.sa_family == 0) {
            addr.sa.sa_family = AF_INET;
        }

		free (pIfinfoList);
        return addr;
	}

    free (pIfinfoList);
	return addr;
}

/*
 *  	osiSockDiscoverBroadcastAddresses ()
 */
epicsShareFunc void epicsShareAPI osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr)
{
	struct sockaddr_in 	*pInetAddr;
	struct sockaddr_in 	*pInetNetMask;
	int             	status;
	INTERFACE_INFO      *pIfinfo;
	INTERFACE_INFO      *pIfinfoList;
	unsigned			nelem;
	int					numifs;
	DWORD				cbBytesReturned;
    osiSockAddrNode     *pNewNode;

	/* only valid for winsock 2 and above */
	if (wsaMajorVersion() < 2 ) {
		fprintf(stderr, "Need to set EPICS_CA_AUTO_ADDR_LIST=NO for winsock 1\n");
		return;
	}

	nelem = 10;
	pIfinfoList = (INTERFACE_INFO *) calloc(nelem, sizeof(INTERFACE_INFO));
	if(!pIfinfoList){
		return;
	}

	status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST, 
						NULL, 0,
						(LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
						&cbBytesReturned, NULL, NULL);

	if (status != 0 || cbBytesReturned == 0) {
		fprintf(stderr, "WSAIoctl SIO_GET_INTERFACE_LIST failed %d\n",WSAGetLastError());
		free(pIfinfoList);		
		return;
	}

	numifs = cbBytesReturned/sizeof(INTERFACE_INFO);
	for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

		/*
		 * dont bother with interfaces that have been disabled
		 */
		if (!(pIfinfo->iiFlags & IFF_UP)) {
			continue;
		}

		/*
		 * dont use the loop back interface
		 */
		if (pIfinfo->iiFlags & IFF_LOOPBACK) {
			continue;
		}

		pInetAddr = (struct sockaddr_in *) &pIfinfo->iiAddress;
		pInetNetMask = (struct sockaddr_in *) &pIfinfo->iiNetmask;

		/*
		 * work around WS2 bug
		 */
		if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
			if (pIfinfo->iiAddress.Address.sa_family == 0) {
				pIfinfo->iiAddress.Address.sa_family = AF_INET;
			}
		}

		/*
		 * if it isnt a wildcarded interface then look for
		 * an exact match
		 */
        if (pMatchAddr->sa.sa_family != AF_UNSPEC) {
            if (pIfinfo->iiAddress.Address.sa_family != pMatchAddr->sa.sa_family) {
                continue;
            }
            if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->sa.sa_family != AF_INET) {
                continue;
            }
		    if (pMatchAddr->ia.sin_addr.s_addr != htonl(INADDR_ANY)) {
			    if (pIfinfo->iiAddress.AddressIn.sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr) {
				    continue;
			    }
		    }
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            errlogPrintf ("osiSockDiscoverInterfaces(): no memory available for configuration\n");
            return;
        }

        if (pIfinfo->iiAddress.Address.sa_family == AF_INET && pIfinfo->iiFlags & IFF_BROADCAST) {
            const unsigned mask = pIfinfo->iiNetmask.AddressIn.sin_addr.s_addr;
            const unsigned bcast = pIfinfo->iiBroadcastAddress.AddressIn.sin_addr.s_addr;
            const unsigned addr = pIfinfo->iiAddress.AddressIn.sin_addr.s_addr;
            unsigned result = (addr & mask) | (bcast &~mask);
            pNewNode->addr.ia.sin_family = AF_INET;
            pNewNode->addr.ia.sin_addr.s_addr = result;
        } 
        else {
            pNewNode->addr.sa = pIfinfo->iiBroadcastAddress.Address;
        }

		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
	}

	free (pIfinfoList);
}
