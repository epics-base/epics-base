/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* bsdSockResource.c */
/* share/src/libCom/os/generic/$Id$ */
/*
 *      Author:		Jeff Hill 
 *      Date:          	04-05-94 
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "bsdSocketResource.h"
#include "epicsAssert.h"

/*
 * NOOP
 */
int bsdSockAttach()
{
	return 1;
}

/*
 * NOOP
 */
void bsdSockRelease()
{
}

/*
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
void epicsShareAPI ipAddrToA 
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
			sprintf (pBuf, "%.*s:%hu", (int) (bufSize-maxPortDigits-1), 
				pString, ntohs(paddr->sin_port));
		}
		else {
			sprintf (pBuf, "%.*s", (int) (bufSize-1), pString);
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
