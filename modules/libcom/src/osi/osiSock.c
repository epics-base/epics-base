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

#ifdef AF_INET6
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#define nDigitsDottedIP 4u
#define chunkSize 8u

#define makeMask(NBITS) ( ( 1u << ( (unsigned) NBITS) ) - 1u )

static void osiSockIPv4toIPv6(const osiSockAddr46 *pAddr46Input,
                              osiSockAddr46 *pAddr46Output)
{
#ifdef AF_INET6
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
#ifdef AF_INET6
    if ( paddr->sa_family == AF_INET6 ) {
        unsigned ret;
        int gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6), pBuf, bufSize,
                                    NULL, 0,  NI_NAMEREQD);
        if (!gai_ecode) {
            return (unsigned)strlen(pBuf);
        }
        ret = sockAddrToDottedIP ( paddr, pBuf, bufSize );
#ifdef NETDEBUG
        epicsBaseDebugLog ( "getnameinfo(%s) failed: %s\n",
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
#ifdef AF_INET6
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
#if defined(AF_INET6) && defined(NI_MAXHOST) && defined(NI_MAXSERV)
    } else if (pAddr->sin_family == AF_INET6) {
        const struct sockaddr * paddr = (const struct sockaddr * )pAddr;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        int flags = NI_NUMERICHOST | NI_NUMERICSERV;
        int gai_ecode;
        gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6),
                                hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), flags);
        if (gai_ecode) {
            snprintf(pBuf, bufSize, "getnameinfo error %s", gai_strerror(gai_ecode));
            return (unsigned)strlen(pBuf);
        }
        snprintf(pBuf, bufSize, "[%s]:%s", hbuf, sbuf);
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
#ifdef AF_INET6
    } else if (domain == AF_INET6) {
        domain_family_str = "AF_INET6";
#endif
    }
    if (type == SOCK_DGRAM) {
        type_str = "DGRAM";
    } else if (type == SOCK_STREAM) {
        type_str = "STREAM";
    }
    SOCKET sock = epicsSocketCreate( domain, type, protocol );
    epicsBaseDebugLogFL("epicsSocketCreate(%s:%d) (%s %s) socket=%d\n",
                    filename, lineno,
                    domain_family_str, type_str, (int)sock);
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
#ifdef AF_INET6
    if (pAddr46->sa.sa_family == AF_INET6) {
      socklen = ( osiSocklen_t ) sizeof ( pAddr46->in6 ) ;
    }
#endif
    status = bind(sock, &pAddr46->sa, socklen);
#ifdef NETDEBUG
    /* if (status < 0) */ {
        char buf[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
        epicsBaseDebugLogFL("%s:%d: bind(%d) address='%s' socklen=%u status=%d: %s\n",
                        filename, lineno,
                        (int)sock,
                        buf, (unsigned)socklen,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall epicsSocket46BindLocalPortFL(const char* filename, int lineno,
                                                         SOCKET sock,
                                                         int sockets_family,
                                                         unsigned short port)

{
    osiSockAddr46 addr46;
    memset ( (char *)&addr46, 0 , sizeof (addr46) );
    addr46.sa.sa_family = sockets_family;
#ifdef AF_INET6
    if ( sockets_family == AF_INET6 ) {
        addr46.in6.sin6_addr = in6addr_any;
        addr46.in6.sin6_port = htons ( port );
    }
    else
#endif
    {
        addr46.ia.sin_addr.s_addr = htonl ( INADDR_ANY );
        addr46.ia.sin_port = htons ( port );
    }
    return epicsSocket46BindFL(filename, lineno, sock, &addr46);
}


/*
 * Wrapper around connect()
 * Allows to connect to an IPv4 address on an IPv6 socket
 */
LIBCOM_API int epicsStdCall epicsSocket46ConnectFL(const char *filename, int lineno,
                                                   SOCKET sock,
                                                   int sockets_family,
                                                   const osiSockAddr46 *pAddr46)
{
    osiSockAddr46 addr46Output = *pAddr46; /* struct copying */
    int status;
    osiSocklen_t socklen = sizeof(addr46Output.ia);
#ifdef AF_INET6
    /*  If needed (socket is created with AF_INET6),
        convert an IPv4 address into a IPv4 mapped address */
    if (sockets_family == AF_INET6 && pAddr46->sa.sa_family == AF_INET) {
        osiSockIPv4toIPv6(pAddr46, &addr46Output);
    }
    if (addr46Output.sa.sa_family == AF_INET6) {
        socklen = sizeof(addr46Output.in6);
    }
#endif

    status = connect(sock, &addr46Output.sa, socklen);

#ifdef NETDEBUG
    {
        char bufIn[64];
        char bufOut[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufIn, sizeof(bufIn));
        sockAddrToDottedIP(&addr46Output.sa, bufOut, sizeof(bufOut));
        epicsBaseDebugLogFL("%s:%d: connect(%d) address='%s' (%s) status=%d %s\n",
                        filename, lineno,
                        (int)sock,
                        bufIn,
                        pAddr46->sa.sa_family != addr46Output.sa.sa_family ? bufOut : "",
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
    }
#endif
    return status;
}

LIBCOM_API int epicsStdCall epicsSocket46RecvFL(const char *filename, int lineno,
                                                SOCKET sock,
                                                void* buf, size_t len, int flags)
{
    int status;
    status = recv(sock, buf, len, flags ) ;

#ifdef NETDEBUG
    {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        epicsBaseDebugLogFL("%s:%d: recv(%d) len=%u status=%d %s\n",
                        filename, lineno,
                        (int)sock, (unsigned)len,
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
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        epicsBaseDebugLogFL("%s:%d: send(%d) len=%u status=%d %s\n",
                        filename, lineno,
                        (int)sock, (unsigned)len,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
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
#ifdef AF_INET6
    if (pAddr46->sa.sa_family == AF_INET6) {
      socklen = ( osiSocklen_t ) sizeof ( pAddr46->in6 ) ;
    }
#endif
    status = sendto(sock, buf, len, flags, &pAddr46->sa, socklen ) ;

#ifdef NETDEBUG
    {
        char bufIn[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufIn, sizeof(bufIn));
        epicsBaseDebugLogFL("%s:%d: sendto(%d) address='%s' len=%u status=%d %s\n",
                        filename, lineno,
                        (int)sock, bufIn, (unsigned)len,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
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
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, bufDotted, sizeof(bufDotted));
        epicsBaseDebugLogFL("%s:%d: recvfrom(%d) buflen=%u address='%s' status=%d %s\n",
                        filename, lineno,
                        (int)sock, (unsigned)len, bufDotted,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
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
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(&pAddr46->sa, buf, sizeof(buf));
        epicsBaseDebugLogFL("%s:%d: accept(%d) address='%s' status=%d (%s)\n",
                        filename, lineno,
                        (int)sock, buf,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
    }
#endif
    return status;
}


LIBCOM_API int epicsStdCall sockIPsAreIdentical46(const osiSockAddr46 *pAddr1,
                                                  const osiSockAddr46 *pAddr2)
{
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_addr.s_addr == pAddr2->ia.sin_addr.s_addr) {
        return 1;
#ifdef AF_INET6
    } else if (pAddr1->sa.sa_family == AF_INET6 && pAddr2->sa.sa_family == AF_INET6 &&
               !memcmp(&pAddr1->in6.sin6_addr, &pAddr2->in6.sin6_addr,
                       sizeof(pAddr2->in6.sin6_addr))&&
               pAddr1->in6.sin6_scope_id == pAddr2->in6.sin6_scope_id) {
        return 1;
#endif
    }
    return 0;
}

LIBCOM_API int epicsStdCall sockAddrAreIdentical46(const osiSockAddr46 *pAddr1,
                                                   const osiSockAddr46 *pAddr2)
{
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_addr.s_addr == pAddr2->ia.sin_addr.s_addr &&
        pAddr1->ia.sin_port == pAddr2->ia.sin_port) {
        return 1;
#ifdef AF_INET6
    } else if (pAddr1->sa.sa_family == AF_INET6 && pAddr2->sa.sa_family == AF_INET6 &&
               !memcmp(&pAddr1->in6.sin6_addr, &pAddr2->in6.sin6_addr,
                       sizeof(pAddr2->in6.sin6_addr)) &&
               pAddr1->in6.sin6_port == pAddr2->in6.sin6_port &&
               pAddr1->in6.sin6_scope_id == pAddr2->in6.sin6_scope_id) {
        return 1;
#endif
    }
    return 0;
}

LIBCOM_API int epicsStdCall sockPortAreIdentical46(const osiSockAddr46 *pAddr1,
                                                   const osiSockAddr46 *pAddr2)
{
    if (pAddr1->sa.sa_family == AF_INET && pAddr2->sa.sa_family == AF_INET &&
        pAddr1->ia.sin_port == pAddr2->ia.sin_port) {
        return 1;
#ifdef AF_INET6
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
#ifdef AF_INET6
    } else if (paddr->in6.sin6_family == AF_INET6) {
        return ntohs(paddr->in6.sin6_port);
#endif
    }
#ifndef NETDEBUG
    epicsBaseDebugLog("epicsSocket46portFromAddress invalid family: %d\n",
                  paddr->ia.sin_family);
#endif
    return -1;
}

/*
 * AF_INET or AF_INET6
 */
LIBCOM_API int epicsStdCall epicsSocket46GetDefaultAddressFamily(void)
{
#ifdef AF_INET6
    return AF_INET6;
#else
    return AF_INET;
#endif
}

/*
 * The family is AF_INET or AF_INET6
 */
LIBCOM_API int epicsStdCall epicsSocket46IsAF_INETorAF_INET6(int family)
{
    if (family == AF_INET) return 1;
#ifdef AF_INET6
    if (family == AF_INET6) return 1;
#endif
    return 0;
}

LIBCOM_API SOCKET epicsStdCall
epicsSocket46CreateBindErrMsg(int domain, int type, int protocol,
                              unsigned short     localPort,
                              int                flags,
                              char               *pErrorMessage,
                              size_t             errorMessageSize)
{
    if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
        errlogPrintf("%s:%d: epicsSocket46CreateBind domain/family=%d type=%d protocol=%d localPort=%u flags=0x%x\n",
                     __FILE__, __LINE__,
                     domain, type, protocol, localPort, flags);
    }
    SOCKET fd = epicsSocketCreate(domain, type, protocol);

    if (fd  == INVALID_SOCKET) {
        if (pErrorMessage) {
            epicsSnprintf(pErrorMessage, errorMessageSize,
                          "Can't create socket: %s", strerror(SOCKERRNO));
        }
        if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
            errlogPrintf("%s:%d: Can't create socket: %s\n",
                         __FILE__, __LINE__,
                         strerror(SOCKERRNO));
        }
        return INVALID_SOCKET;
    }

    if (flags & EPICSSOCKET_ENABLEBROADCASTS_FLAG) {
        /*
         * Enable broadcasts if so requested
         */
        int i = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void *)&i, sizeof i) < 0) {
            if (pErrorMessage) {
                epicsSnprintf(pErrorMessage,errorMessageSize,
                              "Can't set socket BROADCAST option: %s",
                              strerror(SOCKERRNO));
            }
            if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
                errlogPrintf("%s:%d: Can't set socket BROADCAST option:  %s\n",
                             __FILE__, __LINE__,
                             strerror(SOCKERRNO));
            }
            epicsSocketDestroy(fd);
            return INVALID_SOCKET;
        }
    }
    if (flags & EPICSSOCKET_ENABLEADDRESSREUSE_FLAG) {
        /*
         * Enable SO_REUSEPORT if so requested
         */
        int i = 1;
#ifdef USE_SO_REUSEADDR
        int sockOpt = SO_REUSEADDR;
#else
        int sockOpt = SO_REUSEPORT;
#endif
        if (setsockopt(fd, SOL_SOCKET, sockOpt, (void *)&i, sizeof i) < 0) {
            if (pErrorMessage) {
                epicsSnprintf(pErrorMessage, errorMessageSize,
                              "Can't set socket SO_REUSEPORT option: %s",
                              strerror(SOCKERRNO));
            }
            if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
                errlogPrintf("%s:%d: Can't set socket SO_REUSEPORT option:  %s\n",
                             __FILE__, __LINE__,
                             strerror(SOCKERRNO));
            }
            epicsSocketDestroy(fd);
            return INVALID_SOCKET;
        }
    }

    if (flags & EPICSSOCKET_BIND_LOCALPORT_FLAG)  {
        osiSockAddr46 addr46;
        int status = -2;
        memset(&addr46, 0 , sizeof(addr46));
        if (domain == AF_INET) {
            addr46.ia.sin_family = domain;
            addr46.ia.sin_addr.s_addr = htonl (INADDR_ANY);
            addr46.ia.sin_port = htons (localPort);
            status = bind(fd, &addr46.sa, sizeof(addr46.sa));
#ifdef AF_INET6
        } else if (domain == AF_INET6) {
            addr46.in6.sin6_family = domain;
            addr46.in6.sin6_port = htons(localPort);
            /* Address of sa, length of in6: */
            status = bind(fd, &addr46.sa, sizeof(addr46.in6));
#endif
        }
        if (status) {
            if (pErrorMessage) {
                epicsSnprintf(pErrorMessage, errorMessageSize,
                              "Can not bind: status=%d %s\n",
                              status, strerror(SOCKERRNO));
            }
            if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
                errlogPrintf("%s:%d: Can't bind status=%d: %s\n",
                             __FILE__, __LINE__,
                             status, strerror(SOCKERRNO));
            }
            epicsSocketDestroy(fd);
            return INVALID_SOCKET;
        }
    }
    return fd;
}

LIBCOM_API SOCKET epicsStdCall
epicsSocket46CreateBindConnect(int domain, int type, int protocol,
                               const char         *hostname,
                               unsigned short     port,
                               unsigned short     localPort,
                               int                flags,
                               char               *pErrorMessage,
                               size_t             errorMessageSize)
{
    struct addrinfo *ai, *ai_to_free;
    struct addrinfo hints;
    int gai;
    SOCKET fd = INVALID_SOCKET;
    char tmpErrorMessage[40];
    char service_ascii [8];
    char *hostname_malloc_delete = NULL;

    if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
        errlogPrintf("%s:%d: epicsSocket46CreateBindConnect domain/family=%d type=%d protocol=%d hostname=%s port=%u flags=0x%x\n",
                     __FILE__, __LINE__,
                     domain, type, protocol,
                     hostname, port, flags);
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = type;
    if (domain == AF_INET6) {
        /* we could find both IPv4 and Ipv6 addresses */
        if (flags & EPICSSOCKET_CONNECT_IPV4)
            hints.ai_family = AF_INET;
        else if (flags & EPICSSOCKET_CONNECT_IPV6)
            hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = domain;
    }
    /* We accept [hostname].
       This allows to specify [example.com] to use IPv6 only */
    if (hostname[0] == '[') {
        char *cp;
        hostname_malloc_delete = epicsStrDup(&hostname[1]);
        cp = strchr(hostname_malloc_delete, ']');
        if (cp) *cp = '\0';
        hints.ai_family = AF_INET6;
    } else if (hostname[0] == ']') {
        hostname_malloc_delete = epicsStrDup(&hostname[1]);
        hints.ai_family = AF_INET;
    }

    /* supply the port number as ASCII*/
    epicsSnprintf(service_ascii, sizeof(service_ascii), "%u", port);
    gai = getaddrinfo(hostname_malloc_delete ? hostname_malloc_delete : hostname,
                      service_ascii, &hints, &ai);
    if (gai) {
        if (pErrorMessage) {
            epicsSnprintf(pErrorMessage, errorMessageSize,
                          "unable to look up hostname %s %s (%s)",
                          hostname,
                          hostname_malloc_delete ? hostname_malloc_delete : "",
                          gai_strerror(gai));
        }
        if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
            errlogPrintf("%s:%d: unable to look up hostname %s %s (%s)\n",
                         __FILE__, __LINE__,
                         hostname,
                         hostname_malloc_delete ? hostname_malloc_delete : "",
                         gai_strerror(gai));
        }
        free(hostname_malloc_delete);
        return INVALID_SOCKET;
    }
    free(hostname_malloc_delete);
    hostname_malloc_delete = NULL;
    /* now loop through the addresses */
    for (ai_to_free = ai; ai; ai = ai->ai_next) {
        memset(tmpErrorMessage, 0, sizeof(tmpErrorMessage));
        fd = epicsSocket46CreateBindErrMsg(ai->ai_family, ai->ai_socktype, ai->ai_protocol,
                                           localPort, flags,
                                           tmpErrorMessage, sizeof(tmpErrorMessage));
        if (fd != INVALID_SOCKET) {
            int status;
            osiSocklen_t socklen = ai->ai_addrlen;
            if (ai->ai_family == AF_INET) socklen = (osiSocklen_t) sizeof(struct sockaddr_in);
            status = connect(fd, ai->ai_addr, ai->ai_addrlen);
            if (flags & EPICSSOCKET_PRINT_DEBUG_FLAG) {
                char ipHostASCII[64];
                memset(ipHostASCII, 0, sizeof(ipHostASCII));
                sockAddrToDottedIP(ai->ai_addr,
                                   ipHostASCII,
                                   sizeof(ipHostASCII) - 1);
                errlogPrintf("%s:%d: connect to hostname %s (%s) domain/family=%d addrlen=%u socklen=%u status=%d (%s)\n",
                             __FILE__, __LINE__,
                             hostname, ipHostASCII,
                             ai->ai_family, (unsigned)ai->ai_addrlen,
                             (unsigned)socklen, status,
                             status ? strerror(SOCKERRNO) : "");
                if (!status) {
                    return fd;
                } else {
                    epicsSocketDestroy(fd);
                    fd = INVALID_SOCKET;
                }
            }
        }
    }
    /*  If things had gone wrong
     *  retrieve the last error message
     */
    if (fd == INVALID_SOCKET && pErrorMessage) {
        epicsSnprintf(pErrorMessage, errorMessageSize,
                      "%s", tmpErrorMessage);
    }
    freeaddrinfo(ai_to_free);
    return fd;
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
#if defined(AF_INET6) && defined(NI_MAXHOST)
    else if (pAddr->sa_family == AF_INET6) {
        const struct sockaddr * paddr = (const struct sockaddr * )pAddr;
        char hbuf[NI_MAXHOST];
        int flags = NI_NUMERICHOST;
        int gai_ecode;
        gai_ecode = getnameinfo(paddr, sizeof(struct sockaddr_in6),
                                hbuf, sizeof(hbuf), NULL, 0, flags);
        if (gai_ecode) {
            snprintf(pBuf, bufSize, "getnameinfo error %s", gai_strerror(gai_ecode));
            return (unsigned)strlen(pBuf);
        }
        snprintf(pBuf, bufSize, "[%s]", hbuf);
        return (unsigned)strlen(pBuf);
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
    epicsBaseDebugLogFL("%s:%d: epicsSocket46optIPv6MultiCast(%d) interfaceIndex=%u\n",
                    filename, lineno,
                    (int)sock, interfaceIndex);
#endif
#ifdef AF_INET6
    {
        int status = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));

        if ( status )
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_IF(%d)status=%d %s\n",
                         filename, lineno,
                         (int)sock,
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
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_HOPS(%d) hops=%d status=%d %s\n",
                         filename, lineno,
                         (int)sock,
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
            epicsPrintf("%s:%d: epicsSocket46optIPv6MultiCast_LOOP(%d) loop_on=%d status=%d %s\n",
                         filename, lineno,
                         (int)sock,
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
            epicsBaseDebugLogFL("%s:%d: epicsSocket46optIPv6MultiCast_GROUP(%d) sizeof(group)=%u status=%d %s\n",
                            filename, lineno,
                            (int)sock, (unsigned)sizeof(group),
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
#ifdef AF_INET6
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
        epicsBaseDebugLogFL("%s:%d: epicsSocket46addr6toMulticastOK address='%s' multicast='%s'\n",
                            filename, lineno,
                            bufIn, bufOut);
    }
#endif
    if (pAddrInterface->sa.sa_family != AF_INET6) {
        return 0; /* error */
    }
#endif
    return 1;
}
