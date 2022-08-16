/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      socket support library generic code for IPv6
 *
 */

#include "epicsAssert.h"
#include "epicsSignal.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsStdlib.h"
#include "errlog.h"
#include "osiSock.h"
#include "epicsSock.h"
#include "epicsBaseDebugLog.h"
#include "epicsBaseDebugLog.h"

#ifdef __cplusplus
extern "C" {
#endif

static void epicsSockIPv4toIPv6(const struct sockaddr *pAddrInput,
                                osiSockAddr46 *pAddr46Output)
{
#ifdef AF_INET6
    if (pAddrInput->sa_family == AF_INET) {
        const struct sockaddr_in * paddrInput4 = (const struct sockaddr_in *) pAddrInput;
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
        memcpy(pAddr46Output, pAddrInput, sizeof(*pAddr46Output));
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
 * Make sure that the right length is passed into bind(),
 * depending on the address family:
 * AF_INET :      sizeof(struct sockaddr_in)
 * AF_INET6 :     sizeof(struct sockaddr_in6)
 * (else) :       0; bind() should fail
 */
LIBCOM_API int epicsStdCall epicsSocket46BindFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                struct sockaddr *pAddr,
                                                osiSocklen_t addrlen)
{
    int status;
    osiSocklen_t socklen = 0; /* default: Should error out further down */
    if (pAddr->sa_family == AF_INET) {
        if (addrlen >= sizeof(struct sockaddr_in)) {
            socklen = (osiSocklen_t) sizeof(struct sockaddr_in);
        }
    }
#ifdef AF_INET6
    else if (pAddr->sa_family == AF_INET6) {
        if (addrlen >= sizeof(struct sockaddr_in6)) {
            socklen = (osiSocklen_t) sizeof(struct sockaddr_in6);
        }
    }
#endif
    status = bind(sock, pAddr, socklen);
#ifndef NETDEBUG
    /*
     * When NETDEBUG is defined, print always.
     *  Otherwise print only on failure of bind()
     *  Note: preserve errno, as the print may change it
     */
    if (status < 0)
#endif
    {
        char buf[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(pAddr, buf, sizeof(buf));
        epicsBaseDebugLogFL("%s:%d: bind(%d) address='%s' addrlen=%u socklen=%u status=%d: %s\n",
                        filename, lineno,
                        (int)sock,
                            buf, (unsigned) addrlen, (unsigned)socklen,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
    }
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
    return epicsSocket46BindFL(filename, lineno, sock, &addr46.sa, sizeof(addr46));
}


/*
 * Wrapper around connect()
 * Allows to connect to an IPv4 address on an IPv6 socket
 */
LIBCOM_API int epicsStdCall epicsSocket46ConnectFL(const char *filename, int lineno,
                                                   SOCKET sock,
                                                   int sockets_family,
                                                   const struct sockaddr *pAddr,
                                                   osiSocklen_t addrlen)
{
    osiSockAddr46 addr46Output;
    int status;
    memcpy(&addr46Output, pAddr, sizeof(addr46Output));
#ifdef AF_INET6
    /*  If needed (socket is created with AF_INET6),
        convert an IPv4 address into a IPv4 mapped address */
    if (sockets_family == AF_INET6 && pAddr->sa_family == AF_INET) {
        epicsSockIPv4toIPv6(pAddr, &addr46Output);
    }
    if (addr46Output.sa.sa_family == AF_INET6) {
        addrlen = sizeof(addr46Output.in6);
    }
#endif
    if (addr46Output.sa.sa_family == AF_INET) {
        addrlen = sizeof(addr46Output.ia);
    }
    status = connect(sock, &addr46Output.sa, addrlen);

#ifdef NETDEBUG
    {
        char bufIn[64];
        char bufOut[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(pAddr, bufIn, sizeof(bufIn));
        sockAddrToDottedIP(&addr46Output.sa, bufOut, sizeof(bufOut));
        epicsBaseDebugLogFL("%s:%d: connect(%d) address='%s' (%s) status=%d %s\n",
                        filename, lineno,
                        (int)sock,
                        bufIn,
                        pAddr->sa_family != addr46Output.sa.sa_family ? bufOut : "",
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
                                                  const struct sockaddr *pAddr,
                                                  osiSocklen_t addrlen)
{
    osiSocklen_t socklen = 0; /* default: Should error out further down */
    int status;
    if (pAddr->sa_family == AF_INET) {
        if (addrlen >= sizeof(struct sockaddr_in)) {
            socklen = (osiSocklen_t) sizeof(struct sockaddr_in);
        }
    }
#ifdef AF_INET6
    else if (pAddr->sa_family == AF_INET6) {
        if (addrlen >= sizeof(struct sockaddr_in6)) {
            socklen = (osiSocklen_t) sizeof(struct sockaddr_in6);
        }
    }
#endif
    status = sendto(sock, buf, len, flags, pAddr, socklen ) ;

#ifdef NETDEBUG
    {
        char bufIn[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(pAddr, bufIn, sizeof(bufIn));
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


#ifdef NETDEBUG
LIBCOM_API int epicsStdCall epicsSocket46AcceptFL(const char *filename, int lineno,
                                                  SOCKET sock,
                                                  struct sockaddr *pAddr,
                                                  osiSocklen_t *pAddrlen)
{
    int status;
    status = epicsSocketAccept(sock, pAddr, pAddrlen);
    {
        char buf[64];
        char sockErrBuf[64];
        int save_errno = errno;
        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
        sockAddrToDottedIP(pAddr, buf, sizeof(buf));
        epicsBaseDebugLogFL("%s:%d: accept(%d) address='%s' status=%d (%s)\n",
                        filename, lineno,
                        (int)sock, buf,
                        status, status < 0 ? sockErrBuf : "");
        errno = save_errno;
    }
    return status;
}
#endif

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

/*
 * Support for poll(), which is not available on every system
 * Implement a wrapper that uses select()
 * In principle, this wrapper is not needed under Linux (and other OS)
 * and poll() can be used directly. Having it here makes sure that
 * this code is tested and working under all supported platforms
 */
#ifdef AF_INET6
int epicsSockPoll(struct epicsSockPollfd fds[], int nfds, int timeout)
{
    fd_set fdset_rd;
    struct timeval tv, *ptv = NULL;
    int i;
    int highest_fd = 0;
    int ret;

    assert(nfds > 0);
    FD_ZERO(&fdset_rd);
    for (i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        if (fd > highest_fd) {
            highest_fd = fd;
        }
        fds[i].revents = 0;
        if (fds[i].events & EPICSSOCK_POLLIN) {
            FD_SET(fd, &fdset_rd);
        }
    }
    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        ptv = &tv;
    }
    ret = select(highest_fd + 1, &fdset_rd, NULL, NULL, ptv);
#ifdef NETDEBUG
        epicsBaseDebugLog ( "epicsSockPoll nfds=%i timeout=%d ret=%d\n",
                            nfds, timeout, ret);
#endif
    if (ret <= 0) {
        return ret; /* Error or no descriptor ready: timeout */
    }
    /* fill out revent */
    for (i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        if (FD_ISSET (fd, &fdset_rd)) {
            fds[i].revents |= EPICSSOCK_POLLIN;
        }
    }
    return ret; /* both poll() and select return the number of "ready fd" */
}

#ifdef __cplusplus
}
#endif

#endif


