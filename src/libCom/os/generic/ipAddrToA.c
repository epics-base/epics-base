/* $Id$ */
/*
 * 	This provides a generic way to convert an IP address into a 
 *	host name.
 *
 *      Author:         Jeff Hill
 *      Date:           04-05-94
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
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <osiSock.h>

/*
 * inetAddrToHostName()
 */
void ipAddrToA (const struct sockaddr_in *pInetAddr, char *pBuf, 
		const unsigned bufSize)
{
	struct hostent  *ent;
	char		*pName;
#	define		maxPortDigits 16u
        char 		tmp[maxPortDigits+1u];

	if (pInetAddr->sin_family != AF_INET) {
		pName = "UKN ADDR FAMILY";
		tmp[0u] = '\0';
	}
	else {
		int port = ntohs(pInetAddr->sin_port);

		ent = gethostbyaddr(
			(char *) &pInetAddr->sin_addr,
			sizeof(pInetAddr->sin_addr),
			AF_INET);
		if(ent){
			pName = ent->h_name;
		}
		else{
			pName = inet_ntoa(pInetAddr->sin_addr);
		}

		sprintf (tmp, "(%d)", port);
	} 

        /*
         * force null termination
         */
        strncpy(pBuf, pName, bufSize-1);
	strncat(pBuf, tmp, bufSize-1);
        pBuf[bufSize-1u] = '\0';
 
        return;
}

