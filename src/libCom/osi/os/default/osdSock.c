/* osdSock.c */
/* $Id$ */
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
#include "osiSock.h"
#include "epicsAssert.h"

#define DEBUG
#ifdef DEBUG
#   define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#   define ifDepenDebugPrintf(argsInParen)
#endif

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

void epicsShareAPI osiSockDiscoverInterfaces
     (ELLLIST *pList, int socket, unsigned short port, const struct in_addr *pMatchAddr)
{
    static const unsigned           nelem = 100;
    struct sockaddr_in              *pInetAddr;
    int                             status;
    struct ifconf                   ifconf;
    struct ifreq                    *pIfreqList;
    struct ifreq                    *pIfreqListEnd;
    struct ifreq                    *pifreq;
    struct ifreq                    *pnextifreq;
    unsigned                        size;
    osiSockAddrNode                 *pNewNode;
     
     /*
      * use pool so that we avoid using too much stack space
      *
      * nelem is set to the maximum interfaces 
      * on one machine here
      */
     pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pifreq) );
     if (!pIfreqList) {
         errlogPrintf ("osiSockDiscoverInterfaces(): no memory to complete request\n");
         return;
     }
     
     ifconf.ifc_len = nelem * sizeof(*pifreq);
     ifconf.ifc_req = pIfreqList;
     status = socket_ioctl (socket, SIOCGIFCONF, &ifconf);
     if (status < 0 || ifconf.ifc_len == 0) {
         errlogPrintf ("osiSockDiscoverInterfaces(): unable to fetch network interface configuration\n");
         free (pIfreqList);
         return;
     }
     
     pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) pIfreqList);
     pIfreqListEnd--;
     
     for (pifreq = pIfreqList; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {
         unsigned flags;

        /*
         * compute the size of the current if req
         * (hopefully works before and after BSD >= 44)
         */
        size = pifreq->ifr_addr.sa_len + sizeof (pifreq->ifr_name);
        if ( size < sizeof(*pifreq)) {
            size = sizeof(*pifreq);
        }

        pnextifreq = (struct ifreq *) (size + (char *) pifreq);

        /*
         * If its not an internet inteface then dont use it 
         */
        if (pifreq->ifr_ifru.ifru_addr.sa_family != AF_INET) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): interface %s was not AF_INET\n", pifreq->ifr_name) );
             continue;
        }

        status = socket_ioctl (socket, SIOCGIFFLAGS, pifreq);
        if (status) {
            errlogPrintf ("osiSockDiscoverInterfaces(): net intf flags fetch for %s failed\n", pifreq->ifr_name);
            continue;
        }
        
        /*
         * flags are now stored in a union
         */
        flags = (unsigned) pifreq->ifr_ifru.ifru_flags;

        /*
         * dont bother with interfaces that have been disabled
         */
        if (!(flags & IFF_UP)) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s was down\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * dont use the loop back interface
         */
        if (flags & IFF_LOOPBACK) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): ignoring loopback interface: %s\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * Fetch the local address for this interface
         */
        status = socket_ioctl (socket, SIOCGIFADDR, pifreq);
        if (status) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): could not obtain addr for %s\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * verify the address family of the interface that was loaded
         */
        if (pifreq->ifr_ifru.ifru_addr.sa_family != AF_INET) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): interface %s was not AF_INET\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * save the interface's IP address
         */
        pInetAddr = (struct sockaddr_in *) &pifreq->ifr_ifru;

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if (pMatchAddr->s_addr != htonl(INADDR_ANY)) {
             if (pInetAddr->sin_addr.s_addr != pMatchAddr->s_addr) {
                 ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s didnt match\n", pifreq->ifr_name) );
                 continue;
             }
        }

        /*
         * If this is an interface that supports
         * broadcast fetch the broadcast address.
         *
         * Otherwise if this is a point to point 
         * interface then use the destination address.
         *
         * Otherwise CA will not query through the 
         * interface.
         */
        if (flags & IFF_BROADCAST) {
            status = socket_ioctl (socket, SIOCGIFBRDADDR, pifreq);
            if (status) {
                errlogPrintf ("osiSockDiscoverInterfaces(): net intf %s: bcast addr fetch fail\n", pifreq->ifr_name);
                continue;
            }
        }
        else if(flags & IFF_POINTOPOINT) {
            status = socket_ioctl(
                         socket, 
                         SIOCGIFDSTADDR, 
                         pifreq);
            if (status) {
                ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s: pt to pt addr fetch fail\n", pifreq->ifr_name) );
                continue;
            }
        }
        else {
            ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s: not pt to pt or bcast\n", pifreq->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s found\n", pifreq->ifr_name) );

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            errlogPrintf ("osiSockDiscoverInterfaces(): no memory available for configuration\n");
            return;
        }

        pNewNode->destAddr = *pInetAddr;
        pNewNode->destAddr.in.sin_port = htons (port);

		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
    }

    free (pIfreqList);
}
