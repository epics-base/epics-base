/************************************************************************* \
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      socket support library generic code
 *
 *      7-1-97  -joh-
 */

#include <stdio.h>
#include <string.h>

#include "epicsAssert.h"
#include "epicsSignal.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsStdlib.h"
#include "errlog.h"
#include "osiSock.h"
#include "epicsBaseDebugLog.h"

#define nDigitsDottedIP 4u
#define chunkSize 8u

#define makeMask(NBITS) ( ( 1u << ( (unsigned) (NBITS)) ) - 1u )

/*
 * sockAddrAreIdentical()
 * (returns true if addresses are identical)
 */
int epicsStdCall sockAddrAreIdentical 
            ( const osiSockAddr *plhs, const osiSockAddr *prhs )
{
    int match;

    if ( plhs->sa.sa_family != prhs->sa.sa_family ) {
        match = 0;
    }
    else if ( plhs->sa.sa_family != AF_INET ) {
        match = 0;
    }
    else if ( plhs->ia.sin_addr.s_addr != prhs->ia.sin_addr.s_addr ) {
        match = 0;
    }
    else if ( plhs->ia.sin_port != prhs->ia.sin_port ) {
        match = 0;
    }
    else {
        match = 1;
    }
    return match;
}

/*
 * sockAddrToA()
 * (convert socket address to ASCII host name)
 */
unsigned epicsStdCall sockAddrToA ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize )
{
    if ( bufSize < 1 ) {
        return 0;
    }
#ifdef AF_INET6_IPV6
    if ( paddr->sa_family == AF_INET6 ) {
        unsigned ret;
        int gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6), pBuf, bufSize,
                                    NULL, 0,  NI_NAMEREQD);
        if (!gai_ecode) {
            return (unsigned)strlen(pBuf);
        }
        ret = sockAddrToDottedIP ( paddr, pBuf, bufSize );
#ifdef NETDEBUG
        epicsBaseDebugLog("NET getnameinfo(%s) failed: %s\n",
			  pBuf, gai_strerror(gai_ecode) );
#endif
        return ret;
    } else
#endif
    if ( paddr->sa_family != AF_INET ) {
        static const char * pErrStr = "<Ukn Addr Type>";
        unsigned len = ( unsigned ) strlen ( pErrStr );
        if ( len < bufSize ) {
            strcpy ( pBuf, pErrStr );
            return len;
        }
        else {
            strncpy ( pBuf, "<Ukn Addr Type>", bufSize-1 );
            pBuf[bufSize-1] = '\0';
            return bufSize-1;
        }
    }
    else {
        const struct sockaddr_in * paddr_in =
            (const struct sockaddr_in *) paddr;
        return ipAddrToA ( paddr_in, pBuf, bufSize );
    }
}

/*
 * ipAddrToA()
 * (convert IP address to ASCII host name)
 */
unsigned epicsStdCall ipAddrToA ( 
    const struct sockaddr_in * paddr, char * pBuf, unsigned bufSize )
{
    unsigned len = ipAddrToHostName (
        & paddr->sin_addr, pBuf, bufSize );
    if ( len == 0 ) {
        len = ipAddrToDottedIP ( paddr, pBuf, bufSize );
    }
    else {
        unsigned reducedSize = bufSize - len;
        int status = epicsSnprintf (
                        &pBuf[len], reducedSize, ":%hu",
                        ntohs (paddr->sin_port) );
        if ( status > 0 ) {
            unsigned portSize = (unsigned) status;
            if ( portSize < reducedSize ) {
                len += portSize;
            }
        }
    }
    return len;
}

/*
 * sockAddrToDottedIP ()
 */
unsigned epicsStdCall sockAddrToDottedIP ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize )
{
    if ( ( paddr->sa_family != AF_INET )
#ifdef AF_INET6_IPV6
         && ( paddr->sa_family != AF_INET6 )
#endif
                                              )  {
        const char * pErrStr = "<Ukn Addr Type>";
        unsigned errStrLen = ( unsigned ) strlen ( pErrStr );
        if ( errStrLen < bufSize ) {
            strcpy ( pBuf, pErrStr );
            return errStrLen;
        }
        else {
            unsigned reducedSize = bufSize - 1u;
            strncpy ( pBuf, pErrStr, reducedSize );
            pBuf[reducedSize] = '\0';
            return reducedSize;
        }
    }
    else {
        const struct sockaddr_in *paddr_in = ( const struct sockaddr_in * ) paddr;
        return ipAddrToDottedIP ( paddr_in, pBuf, bufSize );
    }
}

/*
 * ipAddrToDottedIP ()
 */
unsigned epicsStdCall ipAddrToDottedIP ( 
    const struct sockaddr_in *pAddr, char *pBuf, unsigned bufSize )
{
    static const char * pErrStr = "<IPA>";
    unsigned strLen;
    unsigned i;
    int status = 0;

    if ( bufSize == 0u ) {
        return 0u;
    }

    *pBuf = '\0';
    if (pAddr->sin_family == AF_INET) {
        unsigned chunk[nDigitsDottedIP];
        unsigned addr = ntohl ( pAddr->sin_addr.s_addr );
        for ( i = 0; i < nDigitsDottedIP; i++ ) {
            chunk[i] = addr & makeMask ( chunkSize );
            addr >>= chunkSize;
        }

        /*
         * inet_ntoa() isnt used because it isnt thread safe
         * (and the replacements are not standardized)
         */
        status = epicsSnprintf (
                                pBuf, bufSize, "%u.%u.%u.%u:%hu",
                                chunk[3], chunk[2], chunk[1], chunk[0],
                                ntohs ( pAddr->sin_port ) );
#if defined(AF_INET6_IPV6) && defined(NI_MAXHOST) && defined(NI_MAXSERV)
    } else if (pAddr->sin_family == AF_INET6) {
        const struct sockaddr * paddr = (const struct sockaddr * )pAddr;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        int flags = NI_NUMERICHOST | NI_NUMERICSERV;
        int gai_ecode;
        gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6),
                                hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), flags);
        if (gai_ecode) {
            epicsSnprintf(pBuf, bufSize, "getnameinfo error %s", gai_strerror(gai_ecode));
            return (unsigned)strlen(pBuf);
        }
        epicsSnprintf(pBuf, bufSize, "[%s]:%s", hbuf, sbuf);
        return (unsigned)strlen(pBuf);
    } else {
        /*
         *  This is for debugging only, assuming a struct defined in
         *  https://datatracker.ietf.org/doc/html/rfc3493
         *  2 fam, 2 port, 4 flowinfo, 16 Ipv6, 4 scope
         */
        unsigned char *pRawBytes =(unsigned char *)pAddr;
        status = epicsSnprintf(pBuf, bufSize, "%02x%02x %02x%02x %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                               pRawBytes[0],  pRawBytes[1],
                               pRawBytes[2],  pRawBytes[3],
                               pRawBytes[4],  pRawBytes[5],
                               pRawBytes[6],  pRawBytes[7],
                               pRawBytes[8],  pRawBytes[9],
                               pRawBytes[10], pRawBytes[11],
                               pRawBytes[12], pRawBytes[13],
                               pRawBytes[14], pRawBytes[15],
                               pRawBytes[16], pRawBytes[17],
                               pRawBytes[18], pRawBytes[19],
                               pRawBytes[20], pRawBytes[21],
                               pRawBytes[22], pRawBytes[23]
                               );
#endif
    }
    if ( status > 0 ) {
        strLen = ( unsigned ) status;
        if ( strLen < bufSize - 1 ) {
            return strLen;
        }
    }
    strLen = ( unsigned ) strlen ( pErrStr );
    if ( strLen < bufSize ) {
        strcpy ( pBuf, pErrStr );
        return strLen;
    }
    else {
        strncpy ( pBuf, pErrStr, bufSize );
        pBuf[bufSize-1] = '\0';
        return bufSize - 1u;
    }
}

/*
 * The "old" osiSockDiscoverBroadcastAddresses() returning
 * only IPv4 in the "osiSockAddrNode" style
 */
LIBCOM_API void epicsStdCall osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    ELLLIST list46 = ELLLIST_INIT;
    osiSockAddr46 addr46;
    addr46.ia = pMatchAddr->ia; /* struct copy */
    osiSockBroadcastMulticastAddresses46(&list46, socket, &addr46);

    osiSockAddrNode   *pNewNode;
    osiSockAddrNode46 *pNode46 = (osiSockAddrNode46*)ellFirst(&list46);
    while (pNode46) {
#ifdef NETDEBUG
        {
            char buf[64];
            sockAddrToDottedIP(&pNode46->sa, buf, sizeof(buf));
            epicsBaseDebugLog("NET osiSockDiscoverBroadcastAddresses '%s'\n", buf );
        }
#endif
        if(pNode46->addr.ia.sin_family == AF_INET) {
            pNewNode = (osiSockAddrNode *) calloc (1, sizeof (*pNewNode) );
            pNewNode->addr.sa.sa_family = pNode46->addr.sa.sa_family;
            pNewNode->addr.ia = pNode46->addr.ia; /* struct copy */
            ellAdd (pList, &pNewNode->node );
        }
        pNode46 = (osiSockAddrNode46*)ellNext(&pNode46->node);
    }
    ellFree(&list46);
}
