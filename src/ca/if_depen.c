/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* if_depen.c */
/* share/src/ca/$Id$ */

/*
 *      Author:	Jeff Hill 
 *      Date:  	04-05-94	
 *      Experimental Physics and Industrial Control System (EPICS)
 */

static char	*sccsId = "@(#) $Id$";

#include "iocinf.h"

/*
 * Dont use ca_static based lock macros here because this is
 * also called by the server. All locks required are applied at
 * a higher level.
 */
#ifdef DEBUG
#   define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#   define ifDepenDebugPrintf(argsInParen)
#endif
    
/*
 * Move to the next ifreq structure
 * Made difficult by the fact that addresses larger than the structure
 * size may be returned from the kernel.
 */
static struct ifreq * ifreqNext ( struct ifreq *pifreq )
{
    size_t size;

    size = ifreq_size ( pifreq );
    if ( size < sizeof ( *pifreq ) ) {
	    size = sizeof ( *pifreq );
    }

    return ( struct ifreq * )( size + ( char * ) pifreq );
}

/*
 * caDiscoverInterfaces ()
 */
epicsShareFunc void epicsShareAPI caDiscoverInterfaces
	(ELLLIST *pList, SOCKET socket, unsigned short port, struct in_addr matchAddr)
{
    static const unsigned           nelem = 100;
    int                             status;
    struct ifconf                   ifconf;
    struct ifreq                    *pIfreqList;
    struct ifreq                    *pIfreqListEnd;
    struct ifreq                    *pifreq;
    struct ifreq                    *pnextifreq;
    caAddrNode                      *pNewNode;
     
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

    for ( pifreq = pIfreqList; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {

        /*
         * find the next if req
         */
        pnextifreq = ifreqNext (pifreq);

        /*
         * If its not an internet interface then dont use it 
         */
        if ( pifreq->ifr_addr.sa_family != AF_INET ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): interface \"%s\" was not AF_INET\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if ( matchAddr.s_addr != htonl (INADDR_ANY) ) {
             struct sockaddr_in *pInetAddr = (struct sockaddr_in *) &pifreq->ifr_addr;
             if ( pInetAddr->sin_addr.s_addr != matchAddr.s_addr ) {
                 ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf \"%s\" didnt match\n", pifreq->ifr_name) );
                 continue;
             }
        }

        status = socket_ioctl ( socket, SIOCGIFFLAGS, pifreq );
        if ( status ) {
            errlogPrintf ("osiSockDiscoverInterfaces(): net intf flags fetch for \"%s\" failed\n", pifreq->ifr_name);
            continue;
        }
        
        /*
         * dont bother with interfaces that have been disabled
         */
        if ( ! ( pifreq->ifr_flags & IFF_UP ) ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf \"%s\" was down\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * dont use the loop back interface
         */
        if ( pifreq->ifr_flags & IFF_LOOPBACK ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): ignoring loopback interface: \"%s\"\n", pifreq->ifr_name) );
             continue;
        }

        pNewNode = (caAddrNode *) calloc (1, sizeof (*pNewNode) );
        if ( pNewNode == NULL ) {
            errlogPrintf ( "osiSockDiscoverInterfaces(): no memory available for configuration\n" );
            free ( pIfreqList );
            return;
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
        if ( pifreq->ifr_flags & IFF_BROADCAST ) {
            status = socket_ioctl (socket, SIOCGIFBRDADDR, pifreq);
            if ( status ) {
                errlogPrintf ("osiSockDiscoverInterfaces(): net intf \"%s\": bcast addr fetch fail\n", pifreq->ifr_name);
                free ( pNewNode );
                continue;
            }
            pNewNode->destAddr.sa = pifreq->ifr_broadaddr;
        }
        else if ( pifreq->ifr_flags & IFF_POINTOPOINT ) {
            status = socket_ioctl ( socket, SIOCGIFDSTADDR, pifreq);
            if ( status ) {
                ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf \"%s\": pt to pt addr fetch fail\n", pifreq->ifr_name) );
                free ( pNewNode );
                continue;
            }
            pNewNode->destAddr.sa = pifreq->ifr_dstaddr;
        }
        else {
            errlogPrintf ( "osiSockDiscoverInterfaces(): net intf \"%s\": not pt to pt or bcast?\n", pifreq->ifr_name );
            free ( pNewNode );
            continue;
        }

        status = socket_ioctl (socket, SIOCGIFADDR, pifreq);
        if ( status ) {
            errlogPrintf ("osiSockDiscoverInterfaces(): net intf \"%s\": if addr fetch fail\n", pifreq->ifr_name);
            free ( pNewNode );
            continue;
        }

        pNewNode->srcAddr.sa = pifreq->ifr_addr;

        if ( pNewNode->destAddr.sa.sa_family == AF_INET ) {
            pNewNode->destAddr.in.sin_port = htons ( port );
        }

        ifDepenDebugPrintf ( ("osiSockDiscoverInterfaces(): net intf \"%s\" found\n", pifreq->ifr_name) );

		/*
		 * LOCK applied externally
		 */
        ellAdd ( pList, &pNewNode->node );
    }

    free ( pIfreqList );
}
     
/*
 * osiLocalAddr ()
 */
int local_addr (SOCKET socket, struct sockaddr_in *plcladdr)
{
    static const unsigned   nelem = 100;
    static char             init = 0;
    static caAddr           addr;
    int                     status;
    struct ifconf           ifconf;
    struct ifreq            *pIfreqList;
    struct ifreq            *pifreq;
    struct ifreq            *pIfreqListEnd;
    struct ifreq            *pnextifreq;

    if ( init ) {
        *plcladdr = addr.in;
        return OK;
    }

    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.in.sin_family = AF_UNSPEC;
    
    pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pIfreqList) );
    if ( ! pIfreqList ) {
        errlogPrintf ( "osiLocalAddr(): no memory to complete request\n" );
        return ERROR;
    }
 
    ifconf.ifc_len = nelem * sizeof ( *pIfreqList );
    ifconf.ifc_req = pIfreqList;
    status = socket_ioctl ( socket, SIOCGIFCONF, &ifconf );
    if ( status < 0 || ifconf.ifc_len == 0 ) {
        errlogPrintf (
            "CAC: SIOCGIFCONF ioctl failed because %d\n",
            SOCKERRNO );
        free ( pIfreqList );
        return ERROR;
    }
    
    pIfreqListEnd = (struct ifreq *) ( ifconf.ifc_len + (char *) ifconf.ifc_req );
    pIfreqListEnd--;

    for ( pifreq = ifconf.ifc_req; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {
        caAddr addrCpy;

        /*
         * find the next if req
         */
        pnextifreq = ifreqNext ( pifreq );

        if ( pifreq->ifr_addr.sa_family != AF_INET ) {
            ifDepenDebugPrintf ( ("local_addr: interface %s was not AF_INET\n", pifreq->ifr_name) );
            continue;
        }

        addrCpy.sa = pifreq->ifr_addr;

        status = socket_ioctl ( socket, SIOCGIFFLAGS, pifreq );
        if ( status < 0 ) {
            errlogPrintf ( "local_addr: net intf flags fetch for %s failed\n", pifreq->ifr_name );
            continue;
        }

        if ( ! ( pifreq->ifr_flags & IFF_UP ) ) {
            ifDepenDebugPrintf ( ("local_addr: net intf %s was down\n", pifreq->ifr_name) );
            continue;
        }

        /*
         * dont use the loop back interface
         */
        if ( pifreq->ifr_flags & IFF_LOOPBACK ) {
            ifDepenDebugPrintf ( ("local_addr: ignoring loopback interface: %s\n", pifreq->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("local_addr: net intf %s found\n", pifreq->ifr_name) );

        init = 1;
        addr = addrCpy;
        break;
    }

    free ( pIfreqList );

    *plcladdr = addr.in;
    return OK;
}
