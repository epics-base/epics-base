
/* $Id$ */
/*
 *      Author:		Jeff Hill 
 *      Date:       04-05-94 
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
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "epicsAssert.h"
#include "errlog.h"

#define DEBUG
#ifdef DEBUG
#   define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#   define ifDepenDebugPrintf(argsInParen)
#endif
    
/*
 * osiSockDiscoverBroadcastAddresses ()
 */
epicsShareFunc void epicsShareAPI osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    static const unsigned           nelem = 100;
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
        struct sockaddr dest;

#       if BSD >= 44
            size = pifreq->ifr_addr.sa_len + sizeof(pifreq->ifr_name);
            if ( size < sizeof(*pifreq)) {
                size = sizeof(*pifreq);
            }
#       else
            size = sizeof(*pifreq);
#       endif

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
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if (pMatchAddr->sa.sa_family != AF_UNSPEC) {
            if (pifreq->ifr_ifru.ifru_addr.sa_family != pMatchAddr->sa.sa_family) {
                continue;
            }
            if (pifreq->ifr_ifru.ifru_addr.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->sa.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->ia.sin_addr.s_addr != htonl(INADDR_ANY)) {
                 struct sockaddr_in *pInetAddr = (struct sockaddr_in *) &pifreq->ifr_ifru.ifru_addr;
                 if (pInetAddr->sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr) {
                     ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s didnt match\n", pifreq->ifr_name) );
                     continue;
                 }
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
            dest = pifreq->ifr_ifru.ifru_broadaddr;
        }
        else if (flags & IFF_POINTOPOINT) {
            status = socket_ioctl(
                         socket, 
                         SIOCGIFDSTADDR, 
                         pifreq);
            if (status) {
                ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s: pt to pt addr fetch fail\n", pifreq->ifr_name) );
                continue;
            }
            dest = pifreq->ifr_ifru.ifru_dstaddr;
        }
        else {
            ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s: not pt to pt or bcast?\n", pifreq->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf %s found\n", pifreq->ifr_name) );

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            errlogPrintf ("osiSockDiscoverInterfaces(): no memory available for configuration\n");
            free (pIfreqList);
            return;
        }

        pNewNode->addr.sa = dest;

		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
    }

    free (pIfreqList);
}
     
/*
 * osiLocalAddr ()
 */
epicsShareFunc osiSockAddr epicsShareAPI osiLocalAddr (SOCKET socket)
{
    static const unsigned   nelem = 100;
    static char             init = 0;
    static osiSockAddr      addr;
    int                     status;
    struct ifconf           ifconf;
    struct ifreq            *pIfreqList;
    struct ifreq            *pifreq;
    struct ifreq            *pIfreqListEnd;
    struct ifreq            *pnextifreq;
    unsigned                size;

    if (init) {
        return addr;
    }

    memset ( (void *) &addr, '\0', sizeof (addr));
    addr.sa.sa_family = AF_UNSPEC;
    
    pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pIfreqList) );
    if (!pIfreqList) {
        errlogPrintf ("osiLocalAddr(): no memory to complete request\n");
        return addr;
    }
 
    ifconf.ifc_len = nelem * sizeof (*pIfreqList);
    ifconf.ifc_req = pIfreqList;
    status = socket_ioctl (socket, SIOCGIFCONF, &ifconf);
    if (status < 0 || ifconf.ifc_len == 0) {
        errlogPrintf (
            "CAC: SIOCGIFCONF ioctl failed because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO) );
        free (pIfreqList);
        return addr;
    }
    
    pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) ifconf.ifc_req);
    pIfreqListEnd--;

    for (pifreq = ifconf.ifc_req; pifreq<=pIfreqListEnd; pifreq = pnextifreq ) {
        unsigned flags;

        /*
         * compute the size of the current if req
         */
#       if BSD >= 44
            size = pifreq->ifr_addr.sa_len + sizeof(pifreq->ifr_name);
            if ( size < sizeof (*pifreq) ) {
                size = sizeof (*pifreq);
            }
#       else
            size = sizeof (*pifreq);
#       endif

        pnextifreq = (struct ifreq *) (size + (char *) pifreq);

        if (pifreq->ifr_addr.sa_family != AF_INET){
            ifDepenDebugPrintf ( ("local_addr: interface %s was not AF_INET\n", pifreq->ifr_name) );
            continue;
        }

        status = socket_ioctl(socket, SIOCGIFFLAGS, pifreq);
        if (status < 0){
            errlogPrintf ( "local_addr: net intf flags fetch for %s failed\n", pifreq->ifr_name );
            continue;
        }

        /*
         * flags are now stored in a union
         */
        flags = (unsigned) pifreq->ifr_flags;

        if (!(flags & IFF_UP)) {
            ifDepenDebugPrintf ( ("local_addr: net intf %s was down\n", pifreq->ifr_name) );
            continue;
        }

        /*
         * dont use the loop back interface
         */
        if (flags & IFF_LOOPBACK) {
            ifDepenDebugPrintf ( ("local_addr: ignoring loopback interface: %s\n", pifreq->ifr_name) );
            continue;
        }

        status = socket_ioctl (socket, SIOCGIFADDR, pifreq);
        if (status < 0){
            ifDepenDebugPrintf ( ("local_addr: could not obtain addr for %s\n", pifreq->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("local_addr: net intf %s found\n", pifreq->ifr_name) );

        addr.sa = pifreq->ifr_addr;
        init = 1;
        break;
    }

    free (pIfreqList);
    return addr;
}
