/* netdb_depen.c */
/* share/src/ca/$Id$ */
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


static char	*sccsId = "$Id$";


#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>


/*
 * caHostFromInetAddr() 
 */
#ifdef __STDC__
void caHostFromInetAddr(struct in_addr *pnet_addr, char *pBuf, unsigned size)
#else
void caHostFromInetAddr(pnet_addr, pBuf, size)
struct in_addr  *pnet_addr;
char            *pBuf;
unsigned        size;
#endif
{
        char            *pString;
        struct hostent  *ent;

        ent = gethostbyaddr(
                        pnet_addr,
                        sizeof(*pnet_addr),
                        AF_INET);
        if(ent){
                pString = ent->h_name;
        }
        else{
                pString = inet_ntoa(*pnet_addr);
        }

        /*
         * force null termination
         */
        strncpy(pBuf, pString, size-1);
        pBuf[size-1] = '\0';

        return;
}

