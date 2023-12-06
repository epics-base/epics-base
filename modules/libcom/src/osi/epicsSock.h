/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      socket support library API def for IPv6
 */
#ifndef epicsSockh
#define epicsSockh
#include "osiSock.h"

/*
 * Prototypes for the generic wrapper with IPv6 (and debug)
 * All functions use the name "epicsSocket46", to make clear that
 * callers are prepared to use IPv6 (or IPv4 of course)
 * Defining the macro NETDEBUG will enable printouts about socket
 * creation, send(), recv() ... and allows learning and debugging
 */

#ifdef __cplusplus
extern "C" {
#endif

    /*
 * Support for poll(), which is not available on every system
 * Implement a very minimal wrapper that uses select()
 * We could use the native poll() on e.g. Linux or MacOs
 * and may be windows, WSAPoll
 * For the moment use the same code on all platforms
 */

#ifdef AF_INET6_IPV6
#define EPICSSOCK_POLLIN  0x1
//#define POLLOUT 0x2 not implemented yet
struct epicsSockPollfd {
    int   fd;
    short events;
    short revents;
};

LIBCOM_API int epicsStdCall epicsSockPoll(struct epicsSockPollfd fds[], int nfds, int timeout);
LIBCOM_API int epicsSocket46in6AddrIsLinkLocal(const struct sockaddr_in6 *pInetAddr6);
LIBCOM_API int epicsSocket46in6AddrIsMulticast(const struct sockaddr_in6 *pInetAddr6);
#endif

#ifdef NETDEBUG
LIBCOM_API SOCKET epicsStdCall epicsSocket46CreateFL (
    const char *filename, int lineno,
    int domain, int type, int protocol );
#define epicsSocket46Create(a,b,c) epicsSocket46CreateFL(__FILE__, __LINE__,a,b,c)
#else
LIBCOM_API SOCKET epicsStdCall epicsSocket46Create (
    int domain, int type, int protocol );
#endif

#ifdef NETDEBUG
LIBCOM_API int epicsStdCall epicsSocket46AcceptFL (
    const char *filename, int lineno,
    SOCKET sock, struct sockaddr *pAddr, osiSocklen_t *pAddrlen);
#define epicsSocket46Accept(a,b,c) epicsSocket46AcceptFL(__FILE__, __LINE__,a,b,c)
#else
#define epicsSocket46Accept(a,b,c) epicsSocketAccept(a,b,c)
#endif

LIBCOM_API int epicsStdCall sockIPsAreIdentical46(const osiSockAddr46 *pAddr1,
                                                    const osiSockAddr46 *pAddr2);

LIBCOM_API int epicsStdCall sockAddrAreIdentical46(const osiSockAddr46 *pAddr1,
                                                     const osiSockAddr46 *pAddr2);

LIBCOM_API int epicsStdCall sockPortAreIdentical46(const osiSockAddr46 *pAddr1,
                                                     const osiSockAddr46 *pAddr2);

LIBCOM_API int epicsStdCall epicsSocket46portFromAddress(osiSockAddr46 *paddr);

/*
 * attempt to convert ASCII string to an IPv4 or IPv6 address in this order
 * 1) look for traditional doted ip with optional port
 * 2) look for a literal Ipv6 address embeeded in [] with an optional port
 * 3) look for valid host name with optional port
 */
LIBCOM_API int epicsStdCall aToIPAddr46(const char *pAddrString,
                                        unsigned short defaultPort,
                                        osiSockAddr46 *pAddr);

/*
 * Wrapper around bind()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46BindFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                const struct sockaddr *pAddr,
                                                osiSocklen_t addrlen);
#define epicsSocket46Bind(a,b,c) epicsSocket46BindFL(__FILE__, __LINE__, a,b,c)

LIBCOM_API int epicsStdCall epicsSocket46ConnectFL(const char* filename, int lineno,
                                                   SOCKET sock,
                                                   const struct sockaddr *pAddr,
                                                   osiSocklen_t addrlen);
#define epicsSocket46Connect(a,b,c) epicsSocket46ConnectFL(__FILE__, __LINE__, a,b,c)

/*
 * bind a port to a local port
 */
LIBCOM_API int epicsStdCall epicsSocket46BindLocalPortFL(const char* filename, int lineno,
                                                         SOCKET sock,
                                                         int sockets_family,
                                                         unsigned short port);
#define epicsSocket46BindLocalPort(a,b,c) epicsSocket46BindLocalPortFL(__FILE__, __LINE__, a,b,c)


/*
 * Wrapper around recv()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46RecvFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                void* buf, size_t len,
                                                int flags);

#define epicsSocket46Recv(a,b,c,d)  epicsSocket46RecvFL(__FILE__, __LINE__, a,b,c,d)

/*
 * Wrapper around send()
 */
LIBCOM_API int epicsStdCall epicsSocket46SendFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                const void* buf, size_t len,
                                                int flags);

#define epicsSocket46Send(a,b,c,d)  epicsSocket46SendFL(__FILE__, __LINE__, a,b,c,d)
/*
 * Wrapper around sendto()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46SendtoFL(const char* filename, int lineno,
                                                  SOCKET sock,
                                                  const void* buf, size_t len,
                                                  int flags,
                                                  const struct sockaddr *pAddr,
                                                  osiSocklen_t addrlen);

#define epicsSocket46Sendto(a,b,c,d,e,f)  epicsSocket46SendtoFL(__FILE__, __LINE__, a,b,c,d,e,f)

/*
 * Wrapper around recvfrom()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46RecvfromFL(const char* filename, int lineno,
                                                    SOCKET sock,
                                                    void* buf, size_t len,
                                                    int flags,
                                                    struct sockaddr *pAddr,
                                                    osiSocklen_t *pAddrlen);

#define epicsSocket46Recvfrom(a,b,c,d,e,f)  epicsSocket46RecvfromFL(__FILE__, __LINE__, a,b,c,d,e,f)

/*
 * AF_INET or AF_INET6
 */
LIBCOM_API int epicsStdCall epicsSocket46GetDefaultAddressFamily(void);


/*
 * The family is AF_INET or AF_INET6
 */
LIBCOM_API int epicsStdCall epicsSocket46IsAF_INETorAF_INET6(int family);

/*
 * Print an IPv4 address in dotted form, an IPv6 in hex form
 */
LIBCOM_API int epicsSocket46IpOnlyToDotted(const struct sockaddr *pAddr,
                                           char *pBuf, unsigned bufSize);

/*
 * Set the multicast option for an IPv6 socket
 */
LIBCOM_API void epicsSocket46optIPv6MultiCast_FL(const char* filename, int lineno,
                                                 SOCKET sock,
                                                 unsigned int interfaceIndex);
#define epicsSocket46optIPv6MultiCast(a,b) epicsSocket46optIPv6MultiCast_FL(__FILE__, __LINE__,a,b)

LIBCOM_API int epicsSocket46addr6toMulticastOKFL(const char* filename, int lineno,
                                                 const osiSockAddr46 *pAddrInterface,
                                                 osiSockAddr46 *pAddrMulticast);
#define epicsSocket46addr6toMulticastOK(a,b) epicsSocket46addr6toMulticastOKFL(__FILE__, __LINE__,a,b)


#ifdef __cplusplus
}
#endif

#endif /* ifndef epicsSockh */
