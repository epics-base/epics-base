/*************************************************************************\
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
#include "osiDebugPrint.h"

#if EPICS_HAS_IPV6
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#define nDigitsDottedIP 4u
#define chunkSize 8u

#define makeMask(NBITS) ( ( 1u << ( (unsigned) NBITS) ) - 1u )

void osiSockPrint(const char *fileName, int lineNo, const char *format, ...)
{
    va_list pVar;
    va_start(pVar, format);
    vfprintf(stdout, format, pVar);
    va_end(pVar);
}

static void osiSockIPv4toIPv6(const osiSockAddr46 *pAddr46Input,
                              osiSockAddr46 *pAddr46Output)
{
#if EPICS_HAS_IPV6
    if (pAddr46Input->sa.sa_family == AF_INET) {
        const struct sockaddr_in * paddrInput4 = (const struct sockaddr_in *) pAddr46Input;
        struct sockaddr_in6 *pOut6 = (struct sockaddr_in6 *)pAddr46Output;

        unsigned int ipv4addr = htonl ( paddrInput4->sin_addr.s_addr );
        /* RFC3493: IPv4-mapped IPv6 address from RFC 2373 */
        memset(pAddr46Output, 0, sizeof(*pAddr46Output));
        pOut6->sin6_addr.s6_addr[10] = 0xFF;
        pOut6->sin6_addr.s6_addr[11] = 0xFF;
        pOut6->sin6_addr.s6_addr[12] = (ipv4addr >> 24) & 0xFF;
        pOut6->sin6_addr.s6_addr[13] = (ipv4addr >> 16) & 0xFF;
        pOut6->sin6_addr.s6_addr[14] = (ipv4addr >>  8) & 0xFF;
        pOut6->sin6_addr.s6_addr[15] = ipv4addr & 0xFF;
        pOut6->sin6_family = AF_INET6;
        pOut6->sin6_port = paddrInput4->sin_port;
    }
    else
#endif
    {
        memcpy(pAddr46Output, pAddr46Input, sizeof(*pAddr46Output));
    }
}


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
#if EPICS_HAS_IPV6
    if ( paddr->sa_family == AF_INET6 ) {
        unsigned ret;
        int gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6), pBuf, bufSize,
                                    NULL, 0,  NI_NAMEREQD);
        if (!gai_ecode) {
            return (unsigned)strlen(pBuf);
        }
        ret = sockAddrToDottedIP ( paddr, pBuf, bufSize );
#ifdef NETDEBUG
        osiDebugPrint ( "getnameinfo(%s) failed: %s\n",
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
#if EPICS_HAS_IPV6
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
#if EPICS_HAS_IPV6
    } else if (pAddr->sin_family == AF_INET6) {
        struct sockaddr_in6 *pAddr6 = (struct sockaddr_in6 *)pAddr;
        if (IN6_IS_ADDR_V4MAPPED(&pAddr6->sin6_addr)) {
            /* https://datatracker.ietf.org/doc/html/rfc5156 */
            status = epicsSnprintf(pBuf, bufSize, "::FFFF:%u.%u.%u.%u:%hu",
                                   pAddr6->sin6_addr.s6_addr[12],
                                   pAddr6->sin6_addr.s6_addr[13],
                                   pAddr6->sin6_addr.s6_addr[14],
                                   pAddr6->sin6_addr.s6_addr[15],
                                   ntohs(pAddr6->sin6_port));
        } else {
#ifdef IF_NAMESIZE
            char if_name_or_number[IF_NAMESIZE];
            if ( ! if_indextoname((unsigned int)pAddr6->sin6_scope_id, &if_name_or_number[0]) ) {
#else
            char if_name_or_number[16];
            {
#endif
                snprintf(if_name_or_number, sizeof(if_name_or_number),
                         "%u", (unsigned int)pAddr6->sin6_scope_id);
            }
            status = epicsSnprintf (pBuf, bufSize,
                                    "[%x:%x:%x:%x:%x:%x:%x:%x%%%s]:%hu",
                                    (pAddr6->sin6_addr.s6_addr[0] << 8) +
                                    pAddr6->sin6_addr.s6_addr[1],
                                    (pAddr6->sin6_addr.s6_addr[2] << 8) +
                                    pAddr6->sin6_addr.s6_addr[3],
                                    (pAddr6->sin6_addr.s6_addr[4] << 8) +
                                    pAddr6->sin6_addr.s6_addr[5],
                                    (pAddr6->sin6_addr.s6_addr[6] << 8) +
                                    pAddr6->sin6_addr.s6_addr[7],
                                    (pAddr6->sin6_addr.s6_addr[8] << 8) +
                                    pAddr6->sin6_addr.s6_addr[9],
                                    (pAddr6->sin6_addr.s6_addr[10] << 8) +
                                    pAddr6->sin6_addr.s6_addr[11],
                                    (pAddr6->sin6_addr.s6_addr[12] << 8) +
                                    pAddr6->sin6_addr.s6_addr[13],
                                    (pAddr6->sin6_addr.s6_addr[14] << 8) +
                                    pAddr6->sin6_addr.s6_addr[15],
                                    if_name_or_number,
                                    ntohs(pAddr6->sin6_port));
        }
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
 * this version sets the file control flags so that
 * the socket will be closed if the user uses exec()
 * as is the case with third party tools such as TCL/TK
 */
#ifdef NETDEBUG
LIBCOM_API SOCKET epicsStdCall epicsSocket46CreateFL ( 
    const char *filename, int lineno,
    int domain, int type, int protocol )
{
    char *domain_family_str = "";
    char *type_str = "";
    if (domain == AF_INET) {
        domain_family_str = "AF_INET";
    } else if (domain == AF_INET6) {
        domain_family_str = "AF_INET6";
    }
    if (type == SOCK_DGRAM) {
        type_str = "DGRAM";
    } else if (type == SOCK_STREAM) {
        type_str = "STREAM";
    }
    SOCKET sock = epicsSocketCreate( domain, type, protocol );
    epicsPrintf ("epicsSocketCreate(%s:%d) (%s %s) socket=%lu\n",
                 osiDebugStripPath(filename), lineno,
                 domain_family_str, type_str, (unsigned long)sock);
    return sock;
}
#endif

/*
 * Wrapper around bind()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46BindFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                const osiSockAddr46 *pAddr46)
{
    osiSocklen_t socklen = ( osiSocklen_t ) sizeof ( pAddr46->ia ) ;
    int status;
#if EPICS_HAS_IPV6
    if (pAddr46->sa.sa_family == AF_INET6) {
      socklen = ( osiSocklen_t ) sizeof ( pAddr46->in6 ) ;
    }
#endif
    status = bind(sock, &pAddr46->sa, socklen);
#ifdef NETDEBUG
    /* if (status < 0) */ {
        char buf[64];
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
        osiDebugPrint("%s:%d: bind(%lu) address='%s' socklen=%u status=%d: %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      buf, (unsigned)socklen,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall epicsSocket46BindLocalPortFL(const char* filename, int lineno,
                                                         SOCKET sock, unsigned short port)

{
    osiSockAddr46 addr46;
    int addressFamily = epicsSocket46GetDefaultAddressFamily();
    memset ( (char *)&addr46, 0 , sizeof (addr46) );
    addr46.sa.sa_family = addressFamily;
#if EPICS_HAS_IPV6
    if ( addressFamily == AF_INET6 ) {
        addr46.in6.sin6_addr = in6addr_any;
        addr46.in6.sin6_port = htons ( port );
    }
    else
#endif
    {
        addr46.ia.sin_addr.s_addr = htonl ( INADDR_ANY );
        addr46.ia.sin_port = htons ( port );
    }
    return epicsSocket46BindFL(osiDebugStripPath(filename), lineno, sock, &addr46);
}


/*
 * Wrapper around connect()
 */
LIBCOM_API int epicsStdCall epicsSocket46ConnectFL(const char *filename, int lineno,
                                                   SOCKET sock,
                                                   const osiSockAddr46 *pAddr46)
{
    osiSockAddr46 addr46Output;
    int status;
#if EPICS_HAS_IPV6
    osiSocklen_t socklen = sizeof(addr46Output.in6);
#else
    osiSocklen_t socklen = sizeof(addr46Output.ia);
#endif
    osiSockIPv4toIPv6(pAddr46, &addr46Output);
    /* Now we have an IPv6 address. use the size of it when calling connect() */
    status = connect(sock, &addr46Output.sa, socklen);

#ifdef NETDEBUG
    {
        char bufIn[64];
        char bufOut[64];
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufIn, sizeof(bufIn));
        sockAddrToDottedIP(&addr46Output.sa, bufOut, sizeof(bufOut));
        osiDebugPrint("%s:%d: connect(%lu) address='%s' (%s) status=%d %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      bufIn,
                      pAddr46->sa.sa_family != addr46Output.sa.sa_family ? bufOut : "",
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}

LIBCOM_API int epicsStdCall epicsSocket46RecvFL(const char *filename, int lineno,
                                                SOCKET sock,
                                                const void* buf, size_t len, int flags)
{
    int status;
    status = recv(sock, buf, len, flags ) ;

#ifdef NETDEBUG
    {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        osiDebugPrint("%s:%d: recv(%lu) len=%u status=%d %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      (unsigned)len,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}

LIBCOM_API int epicsStdCall epicsSocket46SendFL(const char *filename, int lineno,
                                                SOCKET sock,
                                                const void* buf, size_t len, int flags)
{
    int status;
    status = send(sock, buf, len, flags ) ;

#ifdef NETDEBUG
    {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        osiDebugPrint("%s:%d: send(%lu) len=%u status=%d %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      (unsigned)len,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}

LIBCOM_API int epicsStdCall epicsSocket46SendtoFL(const char *filename, int lineno,
                                                  SOCKET sock,
                                                  const void* buf, size_t len, int flags,
                                                  const osiSockAddr46 *pAddr46)
{
    osiSocklen_t socklen = ( osiSocklen_t ) sizeof ( pAddr46->ia ) ;
    int status;
#if EPICS_HAS_IPV6
    if (pAddr46->sa.sa_family == AF_INET6) {
      socklen = ( osiSocklen_t ) sizeof ( pAddr46->in6 ) ;
    }
#endif
    status = sendto(sock, buf, len, flags, &pAddr46->sa, socklen ) ;

#ifdef NETDEBUG
    {
        char bufIn[64];
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufIn, sizeof(bufIn));
        osiDebugPrint("%s:%d: sendto(%lu) address='%s' len=%u status=%d %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      bufIn,
                      (unsigned)len,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall epicsSocket46RecvfromFL(const char *filename, int lineno,
                                                    SOCKET sock,
                                                    void* buf, size_t len, int flags,
                                                    osiSockAddr46 *pAddr46)
{
    int status;
    osiSocklen_t socklen = sizeof(*pAddr46);
    status = recvfrom(sock, buf, len, flags, &pAddr46->sa, &socklen);
#ifdef NETDEBUG
    {
        char bufDotted[64];
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufDotted, sizeof(bufDotted));
        osiDebugPrint("%s:%d: recvfrom(%lu) buflen=%u address='%s' status=%d %s\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      (unsigned)len, bufDotted,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall epicsSocket46AcceptFL(const char *filename, int lineno,
                                                  SOCKET sock,
                                                  osiSockAddr46 *pAddr46)
{
    int status;
    osiSocklen_t socklen = sizeof(*pAddr46);
    status = epicsSocketAccept(sock, &pAddr46->sa, &socklen);
#ifdef NETDEBUG
    {
        char buf[64];
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
        osiDebugPrint("%s:%d: accept(%lu) address='%s' status=%d (%s)\n",
                      osiDebugStripPath(filename), lineno,
                      (unsigned long)sock,
                      buf,
                      status, status < 0 ? sockErrBuf : "");
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall sockIPsAreIdentical46FL(const char *filename, int lineno,
                                                    const osiSockAddr46 *pAddr1,
                                                    const osiSockAddr46 *pAddr2)
{
    int ret = 0;
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_addr.s_addr == pAddr2->ia.sin_addr.s_addr) {
        ret = 1;
#if EPICS_HAS_IPV6
    } else if (pAddr1->sa.sa_family == AF_INET6 && pAddr2->sa.sa_family == AF_INET6 &&
               !memcmp(&pAddr1->in6.sin6_addr, &pAddr2->in6.sin6_addr,
                       sizeof(pAddr2->in6.sin6_addr))&&
               pAddr1->in6.sin6_scope_id == pAddr2->in6.sin6_scope_id) {
        ret = 1;
#endif
    }
#ifdef NETDEBUG
    {
        char buf1[64];
        char buf2[64];
        sockAddrToDottedIP(&pAddr1->sa, buf1, sizeof(buf1));
        sockAddrToDottedIP(&pAddr2->sa, buf2, sizeof(buf2));
        osiDebugPrint("%s:%d: sockIPsAreIdentical46 addr1='%s' addr2='%s' ret=%d\n",
                      osiDebugStripPath(filename), lineno,
                      buf1, buf2, ret);
    }
#endif
    return ret;
}

LIBCOM_API int epicsStdCall sockAddrAreIdentical46FL(const char *filename, int lineno,
                                                     const osiSockAddr46 *pAddr1,
                                                     const osiSockAddr46 *pAddr2)
{
    int ret = 0;
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_addr.s_addr == pAddr2->ia.sin_addr.s_addr &&
        pAddr1->ia.sin_port == pAddr2->ia.sin_port) {
        ret = 1;
#if EPICS_HAS_IPV6
    } else if (pAddr1->sa.sa_family == AF_INET6 && pAddr2->sa.sa_family == AF_INET6 &&
               !memcmp(&pAddr1->in6.sin6_addr, &pAddr2->in6.sin6_addr,
                       sizeof(pAddr2->in6.sin6_addr)) &&
               pAddr1->in6.sin6_port == pAddr2->in6.sin6_port &&
               pAddr1->in6.sin6_scope_id == pAddr2->in6.sin6_scope_id) {
        ret = 1;
#endif
    }
#ifdef NETDEBUGX2
    {
        char buf1[64];
        char buf2[64];
        sockAddrToDottedIP(&pAddr1->sa, buf1, sizeof(buf1));
        sockAddrToDottedIP(&pAddr2->sa, buf2, sizeof(buf2));
        osiDebugPrint("%s:%d: sockAddrAreIdentical46 addr1='%s' addr2='%s' ret=%d\n",
                      osiDebugStripPath(filename), lineno,
                      buf1, buf2, ret);
    }
#endif
    return ret;
}

LIBCOM_API int epicsStdCall sockPortAreIdentical46FL(const char *filename, int lineno,
                                                     const osiSockAddr46 *pAddr1,
                                                     const osiSockAddr46 *pAddr2)
{
#ifdef NETDEBUG
    {
        char buf1[64];
        char buf2[64];
        sockAddrToDottedIP(&pAddr1->sa, buf1, sizeof(buf1));
        sockAddrToDottedIP(&pAddr2->sa, buf2, sizeof(buf2));
        osiDebugPrint("%s:%d: sockPortAreIdentical46 addr1='%s' addr2='%s'\n",
                      osiDebugStripPath(filename), lineno,
                      buf1, buf2);
    }
#endif
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_port == pAddr2->ia.sin_port) {
        return 1;
#if EPICS_HAS_IPV6
    } else if (pAddr1->sa.sa_family == AF_INET6 && pAddr2->sa.sa_family == AF_INET6 &&
               pAddr1->in6.sin6_port == pAddr2->in6.sin6_port) {
        return 1;
#endif
    }
    return 0;
}

LIBCOM_API int epicsStdCall epicsSocket46portFromAddress(osiSockAddr46 *paddr)
{
    if (paddr->ia.sin_family == AF_INET) {
        return ntohs(paddr->ia.sin_port);
#if EPICS_HAS_IPV6
    } else if (paddr->in6.sin6_family == AF_INET6) {
        return ntohs(paddr->in6.sin6_port);
#endif
    }
#ifndef NETDEBUG
    osiDebugPrint("epicsSocket46portFromAddress invalid family: %d\n",
                  paddr->ia.sin_family);
#endif
    return -1;
}

/*
 * AF_INET or AF_INET6
 */
LIBCOM_API int epicsStdCall epicsSocket46GetDefaultAddressFamily(void)
{
#if EPICS_HAS_IPV6
    return AF_INET6;
#else
    return AF_INET;
#endif
}

/*
 * The family is AF_INET or AF_INET6
 * When EPICS_HAS_IPV6 os not set: Only AF_INET
 */
LIBCOM_API int epicsStdCall epicsSocket46IsAF_INETorAF_INET6(int family)
{
    if (family == AF_INET) return 1;
#if EPICS_HAS_IPV6
    if (family == AF_INET6) return 1;
#endif
    return 0;
}

LIBCOM_API int epicsSocket46IpOnlyToDotted(const struct sockaddr *pAddr,
                                       char *pBuf, unsigned bufSize)
{
    unsigned strLen;
    int status = -1;
    if (!bufSize) return 0;

    *pBuf = '\0';
    if (pAddr->sa_family == AF_INET) {
        const struct sockaddr_in *pIa = (const struct sockaddr_in *)pAddr;
        unsigned addr = ntohl(pIa->sin_addr.s_addr);
        /*
         * inet_ntoa() isnt used because it isnt thread safe
         * (and the replacements are not standardized)
         */
        status = epicsSnprintf(pBuf, bufSize, "%u.%u.%u.%u",
                               (addr >> 24) & 0xFF,
                               (addr >> 16) & 0xFF,
                               (addr >> 8) & 0xFF,
                               (addr) & 0xFF);
    }
#if EPICS_HAS_IPV6
    else if (pAddr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;
        if (IN6_IS_ADDR_V4MAPPED(&pIn6->sin6_addr)) {
            /* https://datatracker.ietf.org/doc/html/rfc5156 */
            status = epicsSnprintf(pBuf, bufSize, "::FFFF:%u.%u.%u.%u",
                                   pIn6->sin6_addr.s6_addr[12],
                                   pIn6->sin6_addr.s6_addr[13],
                                   pIn6->sin6_addr.s6_addr[14],
                                   pIn6->sin6_addr.s6_addr[15]);
        } else {
#ifdef IF_NAMESIZE
            char if_name_or_number[IF_NAMESIZE];
            if ( ! if_indextoname((unsigned int)pIn6->sin6_scope_id, &if_name_or_number[0]) ) {
#else
            char if_name_or_number[16];
            {
#endif
                snprintf(if_name_or_number, sizeof(if_name_or_number),
                         "%u", (unsigned int)pIn6->sin6_scope_id);
            }
            status = epicsSnprintf (pBuf, bufSize,
                                    "[%x:%x:%x:%x:%x:%x:%x:%x%%%s]",
                                    (pIn6->sin6_addr.s6_addr[0] << 8) +
                                    pIn6->sin6_addr.s6_addr[1],
                                    (pIn6->sin6_addr.s6_addr[2] << 8) +
                                    pIn6->sin6_addr.s6_addr[3],
                                    (pIn6->sin6_addr.s6_addr[4] << 8) +
                                    pIn6->sin6_addr.s6_addr[5],
                                    (pIn6->sin6_addr.s6_addr[6] << 8) +
                                    pIn6->sin6_addr.s6_addr[7],
                                    (pIn6->sin6_addr.s6_addr[8] << 8) +
                                    pIn6->sin6_addr.s6_addr[9],
                                    (pIn6->sin6_addr.s6_addr[10] << 8) +
                                    pIn6->sin6_addr.s6_addr[11],
                                    (pIn6->sin6_addr.s6_addr[12] << 8) +
                                    pIn6->sin6_addr.s6_addr[13],
                                    (pIn6->sin6_addr.s6_addr[14] << 8) +
                                    pIn6->sin6_addr.s6_addr[15],
                                    if_name_or_number);
        }
    } else {
        /*
         *  This is for debugging only, assuming a struct defined
         *  See https://datatracker.ietf.org/doc/html/rfc3493
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
    }
#else
    else {
        /*
         *  This is for debugging only, assuming a struct defined
         *  See https://datatracker.ietf.org/doc/html/rfc3493
         *  2 fam, 2 port, 4 IPv4
         */
        unsigned char *pRawBytes =(unsigned char *)pAddr;
        status = epicsSnprintf(pBuf, bufSize, "%02x%02x %02x%02x %02x%02x%02x%02x",
                               pRawBytes[0],  pRawBytes[1],
                               pRawBytes[2],  pRawBytes[3],
                               pRawBytes[4],  pRawBytes[5],
                               pRawBytes[6],  pRawBytes[7]
                               );
    }

#endif
    if (status > 0) {
        strLen = (unsigned) status;
        if (strLen < bufSize - 1) {
            return strLen;
        }
    }
    return -1;
}

LIBCOM_API void epicsSocket46optIPv6MultiCast_FL(const char* filename, int lineno,
                                                 SOCKET sock,
                                                 unsigned int interfaceIndex)
{
#ifdef NETDEBUG
    osiDebugPrint("%s:%d: epicsSocket46optIPv6MultiCast(%lu) interfaceIndex=%u\n",
                  osiDebugStripPath(filename), lineno,
                  (unsigned long)sock, interfaceIndex);
#endif
#if EPICS_HAS_IPV6
    {
        int status = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));

        if ( status )
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_IF(%lu)status=%d %s\n",
                         osiDebugStripPath(filename), lineno,
                         (unsigned long)sock,
                         status, status < 0 ? sockErrBuf : "");
        }
    }
    {
        int hops = 1;
        /* Set TTL of multicast packet */
        int status = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                                (char*)&hops, sizeof(hops));

        if ( status )
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_HOPS(%lu) hops=%d status=%d %s\n",
                         osiDebugStripPath(filename), lineno,
                         (unsigned long)sock,
                         hops,
                         status, status < 0 ? sockErrBuf : "");
        }
    }
    {
        int loop_on = 1;
        int status = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                                (char*)&loop_on, sizeof(loop_on));
        if ( status )
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_LOOP(%lu) loop_on=%d status=%d %s\n",
                         osiDebugStripPath(filename), lineno,
                         (unsigned long)sock,
                         loop_on,
                         status, status < 0 ? sockErrBuf : "");
        }
    }
    {
        struct ipv6_mreq group;
        osiSockAddr46 empty46;
        osiSockAddr46 addr46;
        int status;
        memset(&group, 0, sizeof(group));
        memset(&empty46, 0, sizeof(empty46));
        empty46.sa.sa_family = AF_INET6;
        epicsSocket46addr6toMulticastOK(&empty46, &addr46);
        memcpy(&group.ipv6mr_multiaddr,
               &addr46.in6.sin6_addr.s6_addr,
               sizeof(group.ipv6mr_multiaddr.s6_addr));
        group.ipv6mr_interface = interfaceIndex;
        status = setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                            (char*)&group, sizeof(group));
#ifndef NETDEBUG
        if ( status )
#endif
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
            osiDebugPrint("%s:%d: epicsSocket46optIPv6MultiCast_GROUP(%lu) sizeof(group)=%u status=%d %s\n",
                          osiDebugStripPath(filename), lineno,
                          (unsigned long)sock, (unsigned)sizeof(group),
                          status, status < 0 ? sockErrBuf : "");
            if ( ( status ) && (sizeof(group) >= 20 ) )
            {
                unsigned char *pRawBytes = (unsigned char *)&group;
                epicsPrintf("%s:%d: rawBytesGroup=%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
                                  __FILE__, __LINE__,
                                  pRawBytes[0],
                                  pRawBytes[1],
                                  pRawBytes[2],
                                  pRawBytes[3],
                                  pRawBytes[4],
                                  pRawBytes[5],
                                  pRawBytes[6],
                                  pRawBytes[7],
                                  pRawBytes[8],
                                  pRawBytes[9],
                                  pRawBytes[10],
                                  pRawBytes[11],
                                  pRawBytes[12],
                                  pRawBytes[13],
                                  pRawBytes[14],
                                  pRawBytes[15],
                                  pRawBytes[16],
                                  pRawBytes[17],
                                  pRawBytes[18],
                                  pRawBytes[19]);

            }
        }
    }
#endif
}

LIBCOM_API int epicsSocket46addr6toMulticastOKFL(const char* filename, int lineno,
                                                 const osiSockAddr46 *pAddrInterface,
                                                 osiSockAddr46 *pAddrMulticast)
{
#if EPICS_HAS_IPV6
    struct sockaddr_in6 *pAddr6 = &pAddrMulticast->in6;
    memcpy(pAddrMulticast, pAddrInterface, sizeof(*pAddrMulticast));
    if (pAddrInterface->sa.sa_family == AF_INET6) {
        /*
         * If I understand
         * https://www.rfc-editor.org/rfc/rfc2375.html
         * correctly, we can use "All Nodes Address"
         */
        pAddr6->sin6_addr.s6_addr[0] = 0xff;
        pAddr6->sin6_addr.s6_addr[1] = 0x02;
        pAddr6->sin6_addr.s6_addr[2] = 0;
        pAddr6->sin6_addr.s6_addr[3] = 0;
        pAddr6->sin6_addr.s6_addr[4] = 0;
        pAddr6->sin6_addr.s6_addr[5] = 0;
        pAddr6->sin6_addr.s6_addr[6] = 0;
        pAddr6->sin6_addr.s6_addr[7] = 0;
        pAddr6->sin6_addr.s6_addr[8] = 0;
        pAddr6->sin6_addr.s6_addr[9] = 0;
        pAddr6->sin6_addr.s6_addr[10] = 0;
        pAddr6->sin6_addr.s6_addr[11] = 0;
        pAddr6->sin6_addr.s6_addr[12] = 0;
        pAddr6->sin6_addr.s6_addr[13] = 0;
        pAddr6->sin6_addr.s6_addr[14] = 0;
        pAddr6->sin6_addr.s6_addr[15] = 1;
    }
#ifdef NETDEBUGX2
    {
        char bufIn[64];
        char bufOut[64];
        sockAddrToDottedIP(&pAddrInterface->sa, bufIn, sizeof(bufIn));
        sockAddrToDottedIP(&pAddrMulticast->sa, bufOut, sizeof(bufOut));
        osiDebugPrint("%s:%d: epicsSocket46addr6toMulticastOK address='%s' multicast='%s'\n",
                      osiDebugStripPath(filename), lineno,
                      bufIn, bufOut);
    }
#endif
    if (pAddrInterface->sa.sa_family != AF_INET6) {
        return 0; /* error */
    }
#endif
    return 1;
}
