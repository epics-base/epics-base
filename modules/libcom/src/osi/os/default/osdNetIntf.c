/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author:		Jeff Hill 
 *      Date:       04-05-94 
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "epicsThread.h"

#ifdef DEBUG
#   define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#   define ifDepenDebugPrintf(argsInParen)
#endif

static osiSockAddr      osiLocalAddrResult;
static epicsThreadOnceId osiLocalAddrId = EPICS_THREAD_ONCE_INIT;

/*
 * Determine the size of an ifreq structure
 * Made difficult by the fact that addresses larger than the structure
 * size may be returned from the kernel.
 */
static size_t ifreqSize ( struct ifreq *pifreq )
{
    size_t        size;

    size = ifreq_size ( pifreq );
    if ( size < sizeof ( *pifreq ) ) {
	    size = sizeof ( *pifreq );
    }
    return size;
}

/*
 * Move to the next ifreq structure
 */
static struct ifreq * ifreqNext ( struct ifreq *pifreq )
{
    struct ifreq *ifr;

    ifr = ( struct ifreq * )( ifreqSize (pifreq) + ( char * ) pifreq );
    ifDepenDebugPrintf( ("ifreqNext() pifreq %p, size 0x%x, ifr 0x%p\n", pifreq, (unsigned)ifreqSize (pifreq), ifr) );
    return ifr;
}


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
    osiSockAddrNode                 *pNewNode;

    if ( pMatchAddr->sa.sa_family == AF_INET  ) {
        if ( pMatchAddr->ia.sin_addr.s_addr == htonl (INADDR_LOOPBACK) ) {
            pNewNode = (osiSockAddrNode *) calloc (1, sizeof (*pNewNode) );
            if ( pNewNode == NULL ) {
                errlogPrintf ( "osiSockDiscoverBroadcastAddresses(): no memory available for configuration\n" );
                return;
            }
            pNewNode->addr.ia.sin_family = AF_INET;
            pNewNode->addr.ia.sin_port = htons ( 0 );
            pNewNode->addr.ia.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
            ellAdd ( pList, &pNewNode->node );
            return;
        }
    }
     
    /*
     * use pool so that we avoid using too much stack space
     *
     * nelem is set to the maximum interfaces 
     * on one machine here
     */
    pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pifreq) );
    if (!pIfreqList) {
        errlogPrintf ("osiSockDiscoverBroadcastAddresses(): no memory to complete request\n");
        return;
    }
    
    ifconf.ifc_len = nelem * sizeof(*pifreq);
    ifconf.ifc_req = pIfreqList;
    status = socket_ioctl (socket, SIOCGIFCONF, &ifconf);
    if (status < 0 || ifconf.ifc_len == 0) {
        errlogPrintf ("osiSockDiscoverBroadcastAddresses(): unable to fetch network interface configuration (%d)\n", status);
        free (pIfreqList);
        return;
    }
    
    pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) pIfreqList);
    pIfreqListEnd--;

    for ( pifreq = pIfreqList; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {
        uint32_t  current_ifreqsize;

        /*
         * find the next ifreq
         */
        pnextifreq = ifreqNext (pifreq);
        
        /* determine ifreq size */
        current_ifreqsize = ifreqSize ( pifreq );
        /* copy current ifreq to aligned bufferspace (to start of pIfreqList buffer) */
        memmove(pIfreqList, pifreq, current_ifreqsize);

        ifDepenDebugPrintf (("osiSockDiscoverBroadcastAddresses(): found IFACE: %s len: 0x%x current_ifreqsize: 0x%x \n",
            pIfreqList->ifr_name,
            (unsigned)ifreq_size(pifreq),
            (unsigned)current_ifreqsize));

        /*
         * If its not an internet interface then dont use it 
         */
        if ( pIfreqList->ifr_addr.sa_family != AF_INET ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): interface \"%s\" was not AF_INET\n", pIfreqList->ifr_name) );
             continue;
        }

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if ( pMatchAddr->sa.sa_family != AF_UNSPEC ) {
            if ( pMatchAddr->sa.sa_family != AF_INET ) {
                continue;
            }
            if ( pMatchAddr->ia.sin_addr.s_addr != htonl (INADDR_ANY) ) {
                 struct sockaddr_in *pInetAddr = (struct sockaddr_in *) &pIfreqList->ifr_addr;
                 if ( pInetAddr->sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr ) {
                     ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" didnt match\n", pIfreqList->ifr_name) );
                     continue;
                 }
            }
        }

        status = socket_ioctl ( socket, SIOCGIFFLAGS, pIfreqList );
        if ( status ) {
            errlogPrintf ("osiSockDiscoverBroadcastAddresses(): net intf flags fetch for \"%s\" failed\n", pIfreqList->ifr_name);
            continue;
        }
        ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" flags: %x\n", pIfreqList->ifr_name, pIfreqList->ifr_flags) );

        /*
         * dont bother with interfaces that have been disabled
         */
        if ( ! ( pIfreqList->ifr_flags & IFF_UP ) ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" was down\n", pIfreqList->ifr_name) );
             continue;
        }

        /*
         * dont use the loop back interface 
         */
        if ( pIfreqList->ifr_flags & IFF_LOOPBACK ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): ignoring loopback interface: \"%s\"\n", pIfreqList->ifr_name) );
             continue;
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof (*pNewNode) );
        if ( pNewNode == NULL ) {
            errlogPrintf ( "osiSockDiscoverBroadcastAddresses(): no memory available for configuration\n" );
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
        if ( pIfreqList->ifr_flags & IFF_BROADCAST ) {
            osiSockAddr baddr;
            status = socket_ioctl (socket, SIOCGIFBRDADDR, pIfreqList);
            if ( status ) {
                errlogPrintf ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\": bcast addr fetch fail\n", pIfreqList->ifr_name);
                free ( pNewNode );
                continue;
            }
            baddr.sa = pIfreqList->ifr_broadaddr;
            if (baddr.ia.sin_family==AF_INET && baddr.ia.sin_addr.s_addr != INADDR_ANY) {
                pNewNode->addr.sa = pIfreqList->ifr_broadaddr;
                ifDepenDebugPrintf ( ( "found broadcast addr = %x\n", ntohl ( baddr.ia.sin_addr.s_addr ) ) );
            } else {
                ifDepenDebugPrintf ( ( "Ignoring broadcast addr = \n", ntohl ( baddr.ia.sin_addr.s_addr ) ) );
                free ( pNewNode );
                continue;
            }
        }
#if defined (IFF_POINTOPOINT)
        else if ( pIfreqList->ifr_flags & IFF_POINTOPOINT ) {
            status = socket_ioctl ( socket, SIOCGIFDSTADDR, pIfreqList);
            if ( status ) {
                ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\": pt to pt addr fetch fail\n", pIfreqList->ifr_name) );
                free ( pNewNode );
                continue;
            }
            pNewNode->addr.sa = pIfreqList->ifr_dstaddr;
        }
#endif
        else {
            ifDepenDebugPrintf ( ( "osiSockDiscoverBroadcastAddresses(): net intf \"%s\": not point to point or bcast?\n", pIfreqList->ifr_name ) );
            free ( pNewNode );
            continue;
        }

        ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" found\n", pIfreqList->ifr_name) );

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
static void osiLocalAddrOnce (void *raw)
{
    SOCKET *psocket = raw;
    const unsigned          nelem = 100;
    osiSockAddr             addr;
    int                     status;
    struct ifconf           ifconf;
    struct ifreq            *pIfreqList;
    struct ifreq            *pifreq;
    struct ifreq            *pIfreqListEnd;
    struct ifreq            *pnextifreq;

    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.sa.sa_family = AF_UNSPEC;
    
    pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pIfreqList) );
    if ( ! pIfreqList ) {
        errlogPrintf ( "osiLocalAddr(): no memory to complete request\n" );
        goto fail;
    }
 
    ifconf.ifc_len = nelem * sizeof ( *pIfreqList );
    ifconf.ifc_req = pIfreqList;
    status = socket_ioctl ( *psocket, SIOCGIFCONF, &ifconf );
    if ( status < 0 || ifconf.ifc_len == 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf (
            "osiLocalAddr(): SIOCGIFCONF ioctl failed because \"%s\"\n",
            sockErrBuf );
        goto fail;
    }
    
    pIfreqListEnd = (struct ifreq *) ( ifconf.ifc_len + (char *) ifconf.ifc_req );
    pIfreqListEnd--;

    for ( pifreq = ifconf.ifc_req; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {
        osiSockAddr addrCpy;
        uint32_t  current_ifreqsize;

        /*
         * find the next if req
         */
        pnextifreq = ifreqNext ( pifreq );

        /* determine ifreq size */
        current_ifreqsize = ifreqSize ( pifreq );
        /* copy current ifreq to aligned bufferspace (to start of pIfreqList buffer) */
        memmove(pIfreqList, pifreq, current_ifreqsize);

        if ( pIfreqList->ifr_addr.sa_family != AF_INET ) {
            ifDepenDebugPrintf ( ("osiLocalAddr(): interface %s was not AF_INET\n", pIfreqList->ifr_name) );
            continue;
        }

        addrCpy.sa = pIfreqList->ifr_addr;

        status = socket_ioctl ( *psocket, SIOCGIFFLAGS, pIfreqList );
        if ( status < 0 ) {
            errlogPrintf ( "osiLocalAddr(): net intf flags fetch for %s failed\n", pIfreqList->ifr_name );
            continue;
        }

        if ( ! ( pIfreqList->ifr_flags & IFF_UP ) ) {
            ifDepenDebugPrintf ( ("osiLocalAddr(): net intf %s was down\n", pIfreqList->ifr_name) );
            continue;
        }

        /*
         * dont use the loop back interface
         */
        if ( pIfreqList->ifr_flags & IFF_LOOPBACK ) {
            ifDepenDebugPrintf ( ("osiLocalAddr(): ignoring loopback interface: %s\n", pIfreqList->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("osiLocalAddr(): net intf %s found\n", pIfreqList->ifr_name) );

        osiLocalAddrResult = addrCpy;
        free ( pIfreqList );
        return;
    }

    errlogPrintf (
        "osiLocalAddr(): only loopback found\n");
fail:
    /* fallback to loopback */
    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    osiLocalAddrResult = addr;

    free ( pIfreqList );
}


epicsShareFunc osiSockAddr epicsShareAPI osiLocalAddr (SOCKET socket)
{
    epicsThreadOnce(&osiLocalAddrId, osiLocalAddrOnce, &socket);
    return osiLocalAddrResult;
}
