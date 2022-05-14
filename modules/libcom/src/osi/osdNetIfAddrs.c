/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* This file included from various os/.../osdNetIntf.c */

#include <ctype.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

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
 * osiSockDiscoverBroadcastAddresses ()
 */
LIBCOM_API void epicsStdCall osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    osiSockAddrNode *pNewNode;
    struct ifaddrs *ifa;

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

    struct ifaddrs *ifaddr;
    int result = getifaddrs (&ifaddr);
    if ( result != 0 ) {
        errlogPrintf("osiSockDiscoverBroadcastAddresses(): getifaddrs failed.\n");
        return;
    }

    for ( ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next ) {
        if ( ifa->ifa_addr == NULL ) {
              continue;
        }

        ifDepenDebugPrintf (("osiSockDiscoverBroadcastAddresses(): found IFACE: %s\n",
            ifa->ifa_name));

        /*
         * If its not an internet interface then don't use it
         */
        if ( ifa->ifa_addr->sa_family != AF_INET ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): interface \"%s\" was not AF_INET\n", ifa->ifa_name) );
             continue;
        }

        /*
         * if it isn't a wildcarded interface then look for
         * an exact match
         */
        if ( pMatchAddr->sa.sa_family != AF_UNSPEC ) {
            if ( pMatchAddr->sa.sa_family != AF_INET ) {
                continue;
            }
            if ( pMatchAddr->ia.sin_addr.s_addr != htonl (INADDR_ANY) ) {
                 struct sockaddr_in *pInetAddr = (struct sockaddr_in *) ifa->ifa_addr;
                 if ( pInetAddr->sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr ) {
                     ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" didnt match\n", ifa->ifa_name) );
                     continue;
                 }
            }
        }

        /*
         * don't bother with interfaces that have been disabled
         */
        if ( ! ( ifa->ifa_flags & IFF_UP ) ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" was down\n", ifa->ifa_name) );
             continue;
        }

        /*
         * don't use the loop back interface
         */
        if ( ifa->ifa_flags & IFF_LOOPBACK ) {
             ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): ignoring loopback interface: \"%s\"\n", ifa->ifa_name) );
             continue;
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof (*pNewNode) );
        if ( pNewNode == NULL ) {
            errlogPrintf ( "osiSockDiscoverBroadcastAddresses(): no memory available for configuration\n" );
            freeifaddrs ( ifaddr );
            return;
        }

        /*
         * If this is an interface that supports
         * broadcast use the broadcast address.
         *
         * Otherwise if this is a point to point
         * interface then use the destination address.
         *
         * Otherwise CA will not query through the
         * interface.
         */
        if ( ifa->ifa_flags & IFF_BROADCAST ) {
            osiSockAddr baddr;
            baddr.sa = *ifa->ifa_broadaddr;
            if (baddr.ia.sin_family==AF_INET && baddr.ia.sin_addr.s_addr != INADDR_ANY) {
                pNewNode->addr.sa = *ifa->ifa_broadaddr;
                ifDepenDebugPrintf ( ( "found broadcast addr = %08x\n", ntohl ( baddr.ia.sin_addr.s_addr ) ) );
            } else {
                ifDepenDebugPrintf ( ( "Ignoring broadcast addr = %08x\n", ntohl ( baddr.ia.sin_addr.s_addr ) ) );
                free ( pNewNode );
                continue;
            }
        }
#if defined (IFF_POINTOPOINT)
        else if ( ifa->ifa_flags & IFF_POINTOPOINT ) {
            pNewNode->addr.sa = *ifa->ifa_dstaddr;
        }
#endif
        else {
            ifDepenDebugPrintf ( ( "osiSockDiscoverBroadcastAddresses(): net intf \"%s\": not point to point or bcast?\n", ifa->ifa_name ) );
            free ( pNewNode );
            continue;
        }

        ifDepenDebugPrintf ( ("osiSockDiscoverBroadcastAddresses(): net intf \"%s\" found\n", ifa->ifa_name) );

        /*
         * LOCK applied externally
         */
        ellAdd ( pList, &pNewNode->node );
    }

    freeifaddrs ( ifaddr );
}

/*
 * osiLocalAddrOnce ()
 */
static void osiLocalAddrOnce (void *raw)
{
    struct ifaddrs *ifaddr, *ifa;
    int result = getifaddrs (&ifaddr);
    if ( result != 0 ) {
        errlogPrintf("osiLocalAddrOnce(): getifaddrs failed.\n");
        return;
    }

    for ( ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next ) {
        if ( ifa->ifa_addr == NULL ) {
              continue;
        }

        if ( ! ( ifa->ifa_flags & IFF_UP ) ) {
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): net intf %s was down\n", ifa->ifa_name) );
            continue;
        }

        if ( ifa->ifa_addr->sa_family != AF_INET ) {
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): interface %s was not AF_INET\n", ifa->ifa_name) );
            continue;
        }

        /*
         * don't use the loop back interface
         */
        if ( ifa->ifa_flags & IFF_LOOPBACK ) {
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): ignoring loopback interface: %s\n", ifa->ifa_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("osiLocalAddr(): net intf %s found\n", ifa->ifa_name) );

        osiLocalAddrResult.sa = *ifa->ifa_addr;
        freeifaddrs ( ifaddr );
        return;
    }

    errlogPrintf (
        "osiLocalAddr(): only loopback found\n");
    /* fallback to loopback */
    osiSockAddr addr;
    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    osiLocalAddrResult = addr;

    freeifaddrs ( ifaddr );
}


LIBCOM_API osiSockAddr epicsStdCall osiLocalAddr (SOCKET socket)
{
    epicsThreadOnce(&osiLocalAddrId, osiLocalAddrOnce, &socket);
    return osiLocalAddrResult;
}
