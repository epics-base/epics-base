/* bsdSockResource.c */
/* share/src/libCom/os/generic/$Id$ */
/*
 *      Author:		Jeff Hill 
 *      Date:          	04-05-94 
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
 * Modification Log:
 * -----------------
 * .00  mm-dd-yy        iii     Comment
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
