/* $Id$ */
/*
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
 */

/*
 * NOTES: 
 * 1) vxWorks so far does not have DNS support
 *	(and also has a non-standard host table interface)
 * 	so we dont attempt to find the ascii host name equiv
 * 2) We use inet_ntoa_b() here because inet_ntoa() isnt reentrant
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <osiSock.h>

#include <inetLib.h>

/*
 * ipAddrToA ()
 */
void ipAddrToA (const struct sockaddr_in *pInetAddr, 
		char *pBuf, const unsigned bufSize)
{
	char		pName[INET_ADDR_LEN];
	const unsigned  maxPortDigits = 16u;
	char    	tmp[maxPortDigits+1];


	if (pInetAddr->sin_family != AF_INET) {
		strncpy(pName, "UKN ADDR FAMILY", sizeof(pName)-1);
		pName[sizeof(pName)-1u]='\0';
		tmp[0u] = '\0';
	}
	else {
		int port = ntohs(pInetAddr->sin_port);
		inet_ntoa_b (pInetAddr->sin_addr, pName); 
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

