/*************************************************************************\
* Copyright (c) 2013 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * rational replacement for inet_addr()
 *
 * author: Jeff Hill
 */
#include <stdio.h>
#include <string.h>

#include "epicsTypes.h"
#include "osiSock.h"
#include "epicsStdio.h"
#include "errlog.h"

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif /*NELEMENTS*/

/*
 * addrArrayToUL ()
 */
static int addrArrayToUL ( const unsigned *pAddr,
                          unsigned nElements, struct in_addr *pIpAddr )
{
    unsigned i;
    epicsUInt32 addr = 0ul;

    for ( i=0u; i < nElements; i++ ) {
        if ( pAddr[i] > 0xff ) {
            return -1;
        }
        addr <<= 8;
        addr |= ( epicsUInt32 ) pAddr[i];
    }
    pIpAddr->s_addr = htonl ( addr );

    return 0;
}

/*
 * initIPAddr()
 * !! ipAddr should be passed in in network byte order !!
 * !! port is passed in in host byte order !!
 */
static int initIPAddr ( struct in_addr ipAddr, unsigned port,
                        struct sockaddr_in *pIP )
{
    if ( port > 0xffff ) {
        return -1;
    }
    {
        epicsUInt16 port_16 = (epicsUInt16) port;
        memset (pIP, '\0', sizeof(*pIP));
        pIP->sin_family = AF_INET;
        pIP->sin_port = htons(port_16);
        pIP->sin_addr = ipAddr;
    }
    return 0;
}

/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255, allows the user
 * to specify a port number, and allows also a
 * named host to be specified.
 *
 * Sets the port number to "defaultPort" only if
 * "pAddrString" does not contain an address of the form
 * "n.n.n.n:p or host:p"
 */
LIBCOM_API int epicsStdCall 
aToIPAddr( const char *pAddrString, unsigned short defaultPort,
                struct sockaddr_in *pIP )
{
    int status;
    unsigned addr[4];
    unsigned long rawAddr;
    /*
     * !! change n elements here requires change in format below !!
     */
    char hostName[512];
    char dummy[8];
    unsigned port;
    struct in_addr ina;

    if ( pAddrString == NULL ) {
        return -1;
    }

    /*
     * dotted ip addresses
     */
    status = sscanf ( pAddrString, " %u . %u . %u . %u %7s ",
            addr, addr+1u, addr+2u, addr+3u, dummy );
    if ( status == 4 ) {
        if ( addrArrayToUL ( addr, NELEMENTS ( addr ), & ina ) < 0 ) {
            return -1;
        }
        port = defaultPort;
        return initIPAddr ( ina, port, pIP );
    }

    /*
     * dotted ip addresses and port
     */
    status = sscanf ( pAddrString, " %u . %u . %u . %u : %u %7s",
            addr, addr+1u, addr+2u, addr+3u, &port, dummy );
    if ( status >= 5 ) {
        if ( status > 5 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        if ( addrArrayToUL ( addr, NELEMENTS ( addr ), &ina ) < 0 ) {
            return -1;
        }
        return initIPAddr ( ina, port, pIP );
    }

    /*
     * IP address as a raw number
     */
    status = sscanf ( pAddrString, " %lu %7s ", &rawAddr, dummy );
    if ( status == 1 ) {
        if ( rawAddr > 0xffffffff ) {
            return -1;
        }
        port = defaultPort;
        {
            epicsUInt32 rawAddr_32 = ( epicsUInt32 ) rawAddr;
            ina.s_addr = htonl ( rawAddr_32 );
            return initIPAddr ( ina, port, pIP );
        }
    }

    /*
     * IP address as a raw number, and port
     */
    status = sscanf ( pAddrString, " %lu : %u %7s ", &rawAddr, &port, dummy );
    if ( status >= 2 ) {
        if ( status > 2 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        if ( rawAddr > 0xffffffff ) {
            return -1;
        }
        {
            epicsUInt32 rawAddr_32 = ( epicsUInt32 ) rawAddr;
            ina.s_addr = htonl ( rawAddr_32 );
            return initIPAddr ( ina, port, pIP );
        }
    }


    /*
     * host name string
     */
    status = sscanf ( pAddrString, " %511[^:] %s ", hostName, dummy );
    if ( status == 1 ) {
        port = defaultPort;
        status = hostToIPAddr ( hostName, &ina );
        if ( status == 0 ) {
            return initIPAddr ( ina, port, pIP );
        }
    }

    /*
     * host name string, and port
     */
    status = sscanf ( pAddrString, " %511[^:] : %u %s ", hostName,
                        &port, dummy );
    if ( status >= 2 ) {
        if ( status > 2 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        status = hostToIPAddr ( hostName, &ina );
        if ( status == 0 ) {
            return initIPAddr ( ina, port, pIP );
        }
    }

    return -1;
}


/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255, allows the user
 * to specify a port number, and allows also a
 * named host to be specified.
 *
 * Sets the port number to "defaultPort" only if
 * "pAddrString" does not contain an address of the form
 * "n.n.n.n:p or host:p or [IPv6]:p"
 */
LIBCOM_API int epicsStdCall aToIPAddr46(const char *pAddrString,
                                        unsigned short defaultPort,
                                        osiSockAddr46 *pAddr46,
                                        int flags)
{
    if (pAddrString == NULL) {
        return -1;
    }
#ifdef NETDEBUG
    epicsPrintf("%s:%d: aToIPAddr46 pAddrString='%s' defaultPort=%u flags=0x%x\n",
                __FILE__, __LINE__,
                pAddrString, defaultPort, flags);
#endif
    if (*pAddrString != '[') {
        /* IPv4 */
        int status;
        memset ( pAddr46, 0, sizeof(*pAddr46) ) ;
        pAddr46->sa.sa_family = AF_INET;
        status = aToIPAddr( pAddrString, defaultPort, &pAddr46->ia);
#ifdef NETDEBUG
        {
            char buf[64];
            sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
            epicsPrintf("%s:%d: aToIPAddr46 pAddrString='%s' status=%d addr46='%s'\n",
                        __FILE__, __LINE__,
                        pAddrString, status, buf);
                }
#endif
        return status;
    }
#if EPICS_HAS_IPV6
    else
    {
        char hostName[512];
        char portAscii[8];
        char *pPort = NULL;
        struct addrinfo hints;
        struct addrinfo *ai, *ai_to_free;
        osiSocklen_t socklen = 0;
        int gai;
        epicsSnprintf(portAscii, sizeof(portAscii), "%u", defaultPort);
        memset(pAddr46, 0, sizeof(*pAddr46));
        strncpy(hostName, pAddrString, sizeof(hostName) - 1);
        hostName[sizeof(hostName) - 1] = '\0';
        char *pClosingBracket = strchr(hostName, ']');
        if (pClosingBracket) {
            *pClosingBracket = '\0';
            memmove(hostName, &hostName[1], sizeof(hostName) - 1);
            /* now pClosingBracket may pint to a ':', if any */
            if (*pClosingBracket == ':') {
                pPort = pClosingBracket + 1;
                *pClosingBracket = '\0';
            }
        }
        if (!strlen(hostName) && pPort) {
            errlogPrintf("%s:%d: aToIPAddr46 invalid, probably missing []. pAddrString='%s' hostname='' pPort='%s'\n",
                     __FILE__, __LINE__, pAddrString, pPort ? pPort : "");
            return -1;
        }
#ifdef NETDEBUG
        epicsPrintf("%s:%d: aToIPAddr46 hostName='%s' pPort='%s' portAscii='%s'\n",
                    __FILE__, __LINE__,
                    hostName, pPort ? pPort : "NULL", portAscii);
#endif
        /* After the printout, set pPort to a valid value */
        if (!pPort) pPort = portAscii;
#ifdef NETDEBUG
        epicsPrintf("%s:%d: aToIPAddr46 hostName='%s' pPort='%s'\n",
                __FILE__, __LINE__,
                hostName, pPort ? pPort : "NULL");
#endif
        /* we could find both IPv4 and Ipv6 addresses, but see below */
        memset(&hints, 0, sizeof(hints));

        if (sizeof(*pAddr46) < sizeof(struct sockaddr_in6)) {
            hints.ai_socktype = AF_INET;
            hints.ai_family = AF_INET;
        } else if (flags & EPICSSOCKET_CONNECT_IPV4) {
            hints.ai_family = AF_INET;
        } else if (flags & EPICSSOCKET_CONNECT_IPV6) {
            hints.ai_family = AF_INET6;
        }
        gai = getaddrinfo(hostName, pPort, &hints, &ai);
        if (gai) {
#ifdef NETDEBUG
          errlogPrintf("%s:%d: unable to look up pAddrString '%s' '%s:%s' (%s)\n",
                         __FILE__, __LINE__,
                       pAddrString, hostName, pPort,
                         gai_strerror(gai));
#endif
          return -1;
        }
        for (ai_to_free = ai; ai; ai = ai->ai_next) {
            socklen = ai->ai_addrlen;
            if (socklen <= sizeof(*pAddr46)) {
                memcpy(pAddr46, ai->ai_addr, socklen);
#ifdef NETDEBUG
                {
                    char buf[128];
                    sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
                    epicsPrintf("%s:%d: aToIPAddr46 pAddrString='%s' res=%s socklen=%u\n",
                                __FILE__, __LINE__,
                                pAddrString, buf, (unsigned)socklen);
                }
#endif
                freeaddrinfo(ai_to_free);
                return socklen;
            } else {
                char buf[64];
                sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
                epicsPrintf("%s:%d: aToIPAddr46 pAddrString='%s' osiSockAddr46 too short %u/%u ignore add='%s'\n",
                             __FILE__, __LINE__,
                             pAddrString, (unsigned)sizeof(*pAddr46), socklen, buf);
            }
        }
        freeaddrinfo(ai_to_free);
    }
#endif
    return -1;
}
