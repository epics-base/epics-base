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
#include "epicsSock.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "epicsThread.h"

#ifdef NETDEBUG
#   define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#   define ifDepenDebugPrintf(argsInParen)
#endif

static osiSockAddr46     osiLocalAddrResult;
static epicsThreadOnceId osiLocalAddrId = EPICS_THREAD_ONCE_INIT;



static int matchMatchAddress(const osiSockAddr46 *pAddrToMatch46,
                             const osiSockAddr46 *pAddr46)
{
    int match_ret = 0;
#ifdef NETDEBUG
    char buf1[64];
    char buf2[64];
    const static char* fname = "osiSockBroadcastMulticastAddresses46()";
    epicsSocket46IpOnlyToDotted(&pAddrToMatch46->sa, buf1, sizeof(buf1));
    epicsSocket46IpOnlyToDotted(&pAddr46->sa, buf2, sizeof(buf2));
#endif
    if ( ( pAddrToMatch46->sa.sa_family == AF_INET ) &&
         ( pAddr46->sa.sa_family == AF_INET ) ) {
        if ( pAddrToMatch46->ia.sin_addr.s_addr == htonl (INADDR_ANY) ) {
            /* INADDR_ANY will match all interfaces/addresses */
            match_ret = 1;
        } else {
            const struct sockaddr_in *pInetAddr = &pAddr46->ia;
            if ( pInetAddr->sin_addr.s_addr == pAddrToMatch46->ia.sin_addr.s_addr ) {
                match_ret = 1;
            } else {
                ifDepenDebugPrintf ( ("%s %s:%d: '%s did not match '%s'\n",
                                      fname, __FILE__, __LINE__,
                                      buf1, buf2) );
            }
        }
    }
#ifdef AF_INET6
    else if ( ( pAddrToMatch46->sa.sa_family == AF_INET6 ) &&
              ( pAddr46->sa.sa_family == AF_INET6 ) ) {
        if ( !memcmp(&pAddrToMatch46->in6.sin6_addr, &in6addr_any, sizeof(in6addr_any)) ) {
            /*
             * in6addr_any will match all interfaces/addresses
             * link-local-addresses do match, if they share the same scope_id
             */
            match_ret = 1;
        } else {
            const struct sockaddr_in6 *pInetAddr6 = &pAddrToMatch46->in6;
            if (memcmp(&pAddrToMatch46->in6.sin6_addr,
                        &pInetAddr6->sin6_addr,
                        sizeof(pInetAddr6->sin6_addr) ) ) {
                match_ret = 1;
            } else {
                if (epicsSocket46in6AddrIsLinkLocal(&pAddr46->in6) &&
                    pAddr46->in6.sin6_scope_id == pAddrToMatch46->in6.sin6_scope_id) {
                    if (epicsSocket46in6AddrIsLinkLocal(&pAddrToMatch46->in6))
                        match_ret = 1;
                    else if (epicsSocket46in6AddrIsMulticast(&pAddrToMatch46->in6))
                        match_ret = 1;
                }
            }
        }
        if (!match_ret) {
            ifDepenDebugPrintf ( ("%s %s:%d: '%s did not match '%s'\n",
                                  fname, __FILE__, __LINE__,
                                  buf1, buf2 ) );
        }
    }
#endif
    return match_ret;
}

static int copyRemoteAddressOK(osiSockAddrNode46 *pNewNode,
                               osiSockAddr46 *pRemoteAddr46,
                               const char* broadOrDstName)
{
    char buf[64];
    int ret_ok = 0;
#ifdef NETDEBUG
    const static char* fname = "osiSockBroadcastMulticastAddresses46()";
#endif
    if  ( ! pRemoteAddr46 ) {
        snprintf ( buf, sizeof(buf), "%s", "<NULL>" );
    } else if ( ( pRemoteAddr46->sa.sa_family == AF_INET ) &&
                ( pRemoteAddr46->ia.sin_addr.s_addr != INADDR_ANY ) ) {
        pNewNode->addr.ia = pRemoteAddr46->ia; /* struct copy */
        ret_ok = 1;
    }
#ifdef AF_INET6
    else if ( ( pRemoteAddr46 ) && ( pRemoteAddr46->sa.sa_family == AF_INET6 ) ) {
        pNewNode->addr.in6 = pRemoteAddr46->in6;  /* struct copy */
        ret_ok = 1;
    }
#endif
#ifdef NETDEBUG
    {
        if (pRemoteAddr46) {
            epicsSocket46IpOnlyToDotted(&pRemoteAddr46->sa, buf, sizeof(buf));
        }
        ifDepenDebugPrintf (("%s %s:%d:  %s='%s' ret_ok=%d\n",
                             fname, __FILE__, __LINE__,
                             broadOrDstName, buf, ret_ok));
    }
#endif
    return ret_ok;
}


/*
 * osiSockBroadcastMulticastAddresses46 ()
 */
LIBCOM_API void epicsStdCall osiSockBroadcastMulticastAddresses46
     (ELLLIST *pList, SOCKET socket, const osiSockAddr46 *pMatchAddr46)
{
    osiSockAddrNode46 *pNewNode;
    struct ifaddrs *ifa;
#ifdef NETDEBUG
    const static char* fname = "osiSockBroadcastMulticastAddresses46()";
    {
        char buf[64];
        epicsSocket46IpOnlyToDotted(&pMatchAddr46->sa, buf, sizeof(buf));
        ifDepenDebugPrintf (("%s %s:%d: pMatchAddr46='%s'\n",
                             fname, __FILE__, __LINE__,
                             buf));

    }
#endif
    if ( pMatchAddr46->sa.sa_family == AF_INET  ) {
        if ( pMatchAddr46->ia.sin_addr.s_addr == htonl (INADDR_LOOPBACK) ) {
            pNewNode = (osiSockAddrNode46 *) calloc (1, sizeof (*pNewNode) );
            if ( pNewNode == NULL ) {
                errlogPrintf ( "osiSockBroadcastMulticastAddresses46(): no memory available for configuration\n" );
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
        errlogPrintf("osiSockBroadcastMulticastAddresses46(): getifaddrs failed.\n");
        return;
    }

    for ( ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next ) {
        if ( ifa->ifa_addr == NULL ) {
              continue;
        }

        /*
         * If its not an internet interface then don't use it
         */
        if ( ! ( epicsSocket46IsAF_INETorAF_INET6 ( ifa->ifa_addr->sa_family ) ) ) {
#if 0
             ifDepenDebugPrintf ( ("%s %s:%d: interface \"%s\" was not AF_INET(6)\n",
                                   fname, __FILE__, __LINE__,
                                   ifa->ifa_name) );
#endif
             continue;
        }
#ifdef AF_INET6
        if (ifa->ifa_addr->sa_family == AF_INET6) {
            const struct sockaddr_in6 *pInetAddr6 = (const struct sockaddr_in6 *)ifa->ifa_addr;
            /* rfc 4291 defines "Link-local".
               If there are 1 ore more global addresses on the same interface
               we can ignore them all.
               We only need to add one and only one address to the list.
               Since there is no broadcast in IPv6, the address will
               be changed to the multicast address anyway */
            if (epicsSocket46in6AddrIsLinkLocal(pInetAddr6)) {
                ;
            } else {
                ifDepenDebugPrintf ( ("%s %s:%d: net intf \"%s\" ignore a non link-local IPv6 address\n",
                                      fname, __FILE__, __LINE__,
                                      ifa->ifa_name) );
                continue;
            }
        }
#endif
        ifDepenDebugPrintf (("%s %s:%d: found IFACE: %s flags=0x%08x family=%d%s%s interfaceIndex=%u\n",
                             fname, __FILE__, __LINE__,
                             ifa->ifa_name, ifa->ifa_flags, ifa->ifa_addr->sa_family,
                             ifa->ifa_addr->sa_family == AF_INET ? " (AF_INET)" : "",
                             ifa->ifa_addr->sa_family == AF_INET6 ? " (AF_INET6)" : "",
                             if_nametoindex (ifa->ifa_name)));

#ifdef NETDEBUG
        {
            const struct sockaddr *pSockAddr = (struct sockaddr *) ifa->ifa_addr;
            char buf[64];
            epicsSocket46IpOnlyToDotted(pSockAddr, buf, sizeof(buf));
            ifDepenDebugPrintf (("%s %s:%d: interface  \"%s\" has addr '%s'\n",
                                 fname, __FILE__, __LINE__,
                                 ifa->ifa_name, buf));
        }
#endif
        /*
         * if it isn't a wildcarded interface then look for
         * an exact match
         */
        if ( pMatchAddr46->sa.sa_family != AF_UNSPEC ) {
            osiSockAddr46 addr46;
            int match_ok = 0;
            memset(&addr46, 0, sizeof(addr46));
            if (ifa->ifa_addr->sa_family == AF_INET) {
                 struct sockaddr_in *pInetAddr = (struct sockaddr_in *) ifa->ifa_addr;
                 addr46.ia = *pInetAddr;
                 match_ok = matchMatchAddress(pMatchAddr46, &addr46);
            }
#ifdef AF_INET6
            else if (ifa->ifa_addr->sa_family == AF_INET6) {
                struct sockaddr_in6 *pInetAddr6 = (struct sockaddr_in6 *) ifa->ifa_addr;
                addr46.in6 = *pInetAddr6;
                /* TB: Need to verify, if this is OK */
                addr46.in6.sin6_scope_id = if_nametoindex (ifa->ifa_name);
                match_ok = matchMatchAddress(pMatchAddr46, &addr46);
            }
#endif
            if (!match_ok) {
                ifDepenDebugPrintf ( ("%s %s:%d: net intf \"%s\" did not match\n",
                                      fname, __FILE__, __LINE__,
                                      ifa->ifa_name) );
                continue;
            }

        }

        /*
         * don't bother with interfaces that have been disabled
         */
        if ( ! ( ifa->ifa_flags & IFF_UP ) ) {
             ifDepenDebugPrintf ( ("%s %s:%d: net intf \"%s\" was down\n",
                                   fname, __FILE__, __LINE__,
                                   ifa->ifa_name) );
             continue;
        }

        /*
         * don't use the loop back interface
         */
        if ( ifa->ifa_flags & IFF_LOOPBACK ) {
             ifDepenDebugPrintf ( ("%s %s:%d: ignoring loopback interface: \"%s\"\n",
                                   fname, __FILE__, __LINE__,
                                   ifa->ifa_name) );
             continue;
        }

        pNewNode = (osiSockAddrNode46 *) calloc (1, sizeof (*pNewNode) );
        if ( pNewNode == NULL ) {
            errlogPrintf ( "osiSockDiscoverBroadcastAddresses(): no memory available for configuration\n" );
            freeifaddrs ( ifaddr );
            return;
        }
        pNewNode->interfaceIndex = if_nametoindex (ifa->ifa_name);
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
            if ( ifa->ifa_addr->sa_family == AF_INET )  {
                if (!copyRemoteAddressOK(pNewNode,
                                         (osiSockAddr46 *)ifa->ifa_broadaddr,
                                         "broadcastaddr") ) {
                  free ( pNewNode );
                  pNewNode = NULL;
                }
            }
#ifdef AF_INET6
            else if ( ifa->ifa_addr->sa_family == AF_INET6 )  {
              /*
               * IPv6 has no broadcast, even if IFF_BROADCAST is set
               * we need to use multicast
               */
                osiSockAddr46 addrMulticast46;
                if (epicsSocket46addr6toMulticastOK((osiSockAddr46 *)ifa->ifa_addr,
                                                    &addrMulticast46)) {
                    pNewNode->addr = addrMulticast46; /* struct copy */
                    pNewNode->addr.in6.sin6_scope_id = pNewNode->interfaceIndex;
                    pNewNode->addr.sa.sa_family = ifa->ifa_addr->sa_family;
                } else {
                  free ( pNewNode );
                  pNewNode = NULL;
                }
            }
#endif
        }
#if defined (IFF_POINTOPOINT)
        else if ( ifa->ifa_flags & IFF_POINTOPOINT )  {
          /*
           * It is point-to-point, ifa->ifa_dstaddr is != NULL and
           * the family is valid
           */
          if (!copyRemoteAddressOK(pNewNode,
                                   (osiSockAddr46 *)ifa->ifa_dstaddr,
                                   "POINTOPOINTdestaddr") ) {
                free ( pNewNode );
                pNewNode = NULL;
            }
        }
#endif
        else {
            free ( pNewNode );
            pNewNode = NULL;
        }
        if ( pNewNode == NULL ) {
            ifDepenDebugPrintf ( ( "%s %s:%d: IFACE '%s': ignored, no point to point or bcast?\n",
                                   fname, __FILE__, __LINE__,
                                   ifa->ifa_name ) );
            continue;
        } else {
            const struct sockaddr *pSockAddr = &pNewNode->addr.sa;
            char buf[64];
            epicsSocket46IpOnlyToDotted(pSockAddr, buf, sizeof(buf));
            ifDepenDebugPrintf (("%s %s:%d: IFACE '%s': added to list, family=%d interfaceIndex=%u addr='%s'\n",
                                 fname, __FILE__, __LINE__,
                                 ifa->ifa_name, ifa->ifa_addr->sa_family,
                                 pNewNode->interfaceIndex, buf));
        }
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
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): net intf %s was down\n",
                                  ifa->ifa_name) );
            continue;
        }

        /*
         * don't use the loop back interface
         */
        if ( ifa->ifa_flags & IFF_LOOPBACK ) {
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): ignoring loopback interface: '%s' family=%d\n",
                                  ifa->ifa_name, ifa->ifa_addr->sa_family) );
            continue;
        }

        if ( ! ( epicsSocket46IsAF_INETorAF_INET6 ( ifa->ifa_addr->sa_family ) ) ) {
            ifDepenDebugPrintf ( ("osiLocalAddrOnce(): interface \"%s\" was not AF_INET/AF_INET6 (%d/%d) but %d\n",
                                  ifa->ifa_name, AF_INET, AF_INET6, ifa->ifa_addr->sa_family) );
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
    osiSockAddr46 addr;
    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    osiLocalAddrResult = addr;

    freeifaddrs ( ifaddr );
}


LIBCOM_API osiSockAddr46 epicsStdCall osiLocalAddr (SOCKET socket)
{
    epicsThreadOnce(&osiLocalAddrId, osiLocalAddrOnce, &socket);
    return osiLocalAddrResult;
}
