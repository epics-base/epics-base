/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      socket support library API def
 *
 *      7-1-97  -joh-
 */
#ifndef osiSockh
#define osiSockh

#include "libComAPI.h"
#include "osdSock.h"
#include "ellLib.h"

/* Enable it for debugging */
/* #define NETDEBUG */

/* Needed to compile external modules, like pcas */
#define EPICS_BASE_HAS_OSISOCKADDR46

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct sockaddr_in;
struct in_addr;

typedef union osiSockAddr46 {
    struct sockaddr_in  ia;
    struct sockaddr     sa;
#ifdef AF_INET6
    struct sockaddr_in6 in6;
#endif
} osiSockAddr46;


LIBCOM_API SOCKET epicsStdCall epicsSocketCreate ( 
    int domain, int type, int protocol );
LIBCOM_API int epicsStdCall epicsSocketAccept ( 
    int sock, struct sockaddr * pAddr, osiSocklen_t * addrlen );
LIBCOM_API void epicsStdCall epicsSocketDestroy ( 
    SOCKET );
LIBCOM_API void epicsStdCall 
    epicsSocketEnableAddressReuseDuringTimeWaitState ( SOCKET s );
LIBCOM_API void epicsStdCall 
    epicsSocketEnableAddressUseForDatagramFanout ( SOCKET s );

/*
 * Prototypes for the generic wrapper with IPv6 (and debug)
 */

#ifdef NETDEBUG
LIBCOM_API SOCKET epicsStdCall epicsSocket46CreateFL ( 
    const char *filename, int lineno,
    int domain, int type, int protocol );
#define epicsSocket46Create(a,b,c) epicsSocket46CreateFL(__FILE__, __LINE__,a,b,c)
#else
#define epicsSocket46Create(a,b,c) epicsSocketCreate(a,b,c)
#endif

LIBCOM_API int epicsStdCall epicsSocket46AcceptFL (
    const char *filename, int lineno,
    SOCKET sock, osiSockAddr46 *pAddr46 );
#define epicsSocket46Accept(a,b) epicsSocket46AcceptFL(__FILE__, __LINE__,a,b)

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
 * Make sure that the right length is passed inti bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46BindFL(const char* filename, int lineno,
                                                SOCKET sock,
                                                const osiSockAddr46 *pAddr46);
#define epicsSocket46Bind(a,b) epicsSocket46BindFL(__FILE__, __LINE__, a,b)

LIBCOM_API int epicsStdCall epicsSocket46ConnectFL(const char* filename, int lineno,
                                                   SOCKET sock,
                                                   int sockets_family,
                                                   const osiSockAddr46 *pAddr46);
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
                                                  const osiSockAddr46 *pAddr46);

#define epicsSocket46Sendto(a,b,c,d,e)  epicsSocket46SendtoFL(__FILE__, __LINE__, a,b,c,d,e)

/*
 * Wrapper around recvfrom()
 * Make sure that the right length is passed into bind()
 */
LIBCOM_API int epicsStdCall epicsSocket46RecvfromFL(const char* filename, int lineno,
                                                    SOCKET sock,
                                                    void* buf, size_t len,
                                                    int flags,
                                                    osiSockAddr46 *pAddr46);

#define epicsSocket46Recvfrom(a,b,c,d,e)  epicsSocket46RecvfromFL(__FILE__, __LINE__, a,b,c,d,e)

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
 * Fortunately, on most systems the combination of a shutdown of both
 * directions and or a signal is sufficent to interrupt a blocking send,
 * receive, or connect call. For odd ball systems this is stubbed out in the
 * osi area.
 */
enum epicsSocketSystemCallInterruptMechanismQueryInfo {
    esscimqi_socketCloseRequired,
    esscimqi_socketBothShutdownRequired,
    esscimqi_socketSigAlarmRequired /* NO LONGER USED/SUPPORTED */
};
LIBCOM_API enum epicsSocketSystemCallInterruptMechanismQueryInfo 
        epicsSocketSystemCallInterruptMechanismQuery ();

#ifdef EPICS_PRIVATE_API
/*
 * Some systems (e.g Linux and Windows 10) allow to check the amount
 * of unsent data in the output queue.
 * Returns -1 if the information is not available.
 */
LIBCOM_API int epicsSocketUnsentCount(SOCKET sock);
#endif

/*
 * convert socket address to ASCII in this order
 * 1) look for matching host name and typically add trailing IP port
 * 2) failing that, convert to raw ascii address (typically this is a
 *      dotted IP address with trailing port)
 * 3) failing that, writes "<Ukn Addr Type>" into pBuf
 *
 * returns the number of character elements stored in buffer not
 * including the null termination, but always writes at least a
 * null terminator in the string (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall sockAddrToA (
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize );

/*
 * convert IP address to ASCII in this order
 * 1) look for matching host name and add trailing port
 * 2) convert to raw dotted IP address with trailing port
 *
 * returns the number of character elements stored in buffer not
 * including the null termination, but always writes at least a
 * null terminator in the string (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall ipAddrToA (
    const struct sockaddr_in * pInetAddr, char * pBuf, unsigned bufSize );

/*
 * sockAddrToDottedIP ()
 * typically convert to raw dotted IP address with trailing port
 *
 * returns the number of character elements stored in buffer not
 * including the null termination, but always writes at least a
 * null terminator in the string (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall sockAddrToDottedIP ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize );

/*
 * ipAddrToDottedIP ()
 * convert to raw dotted IP address with trailing port
 *
 * returns the number of character elements stored in buffer not
 * including the null termination, but always writes at least a
 * null terminator in the string (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall ipAddrToDottedIP ( 
    const struct sockaddr_in * paddr, char * pBuf, unsigned bufSize );

/*
 * convert inet address to a host name string
 *
 * returns the number of character elements stored in buffer not
 * including the null termination. This will be zero if a matching
 * host name cant be found.
 *
 * there are many OS specific implementation stubs for this routine
 */
LIBCOM_API unsigned epicsStdCall ipAddrToHostName (
    const struct in_addr * pAddr, char * pBuf, unsigned bufSize );

/*
 * attempt to convert ASCII string to an IP address in this order
 * 1) look for traditional doted ip with optional port
 * 2) look for raw number form of ip address with optional port
 * 3) look for valid host name with optional port
 */
LIBCOM_API int epicsStdCall aToIPAddr
    ( const char * pAddrString, unsigned short defaultPort, struct sockaddr_in * pIP);

/*
 * attempt to convert ASCII host name string with optional port to an IP address
 */
LIBCOM_API int epicsStdCall hostToIPAddr 
    (const char *pHostName, struct in_addr *pIPA);
/*
 * attach to BSD socket library
 */
LIBCOM_API int epicsStdCall osiSockAttach (void); /* returns T if success, else F */

/*
 * release BSD socket library
 */
LIBCOM_API void epicsStdCall osiSockRelease (void);

/*
 * convert socket error numbers to a string
 */
LIBCOM_API void epicsSocketConvertErrorToString (
        char * pBuf, unsigned bufSize, int error );
LIBCOM_API void epicsSocketConvertErrnoToString (
        char * pBuf, unsigned bufSize );

typedef union osiSockAddr {
    struct sockaddr_in  ia;
    struct sockaddr     sa;
} osiSockAddr;

typedef struct osiSockAddrNode {
    ELLNODE node;
    osiSockAddr46 addr46;
    unsigned int interfaceIndex; /* Needed for IPv6 only */
} osiSockAddrNode;

/*
 * sockAddrAreIdentical()
 * (returns true if addresses are identical)
 */
LIBCOM_API int epicsStdCall sockAddrAreIdentical 
    ( const osiSockAddr * plhs, const osiSockAddr * prhs );

/*
 *  osiSockDiscoverBroadcastAddresses ()
 *  Returns the broadcast addresses of each network interface found.
 *
 *  This routine is provided with the address of an ELLLIST, a socket,
 *  a destination port number, and a match address. When the
 *  routine returns there will be one additional entry
 *  (an osiSockAddrNode) in the list for each network interface found that
 *  is up and isn't a loop back interface (match addr is INADDR_ANY),
 *  or only the interfaces that match the specified addresses (match addr
 *  is other than INADDR_ANY). If the interface supports broadcasting
 *  then add its broadcast address to the list. If the interface is a
 *  point to point link then add the destination address of the point to
 *  point link to the list.
 *
 *  Any mutex locking required to protect pList is applied externally.
 *
 */
LIBCOM_API void epicsStdCall osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr46 *pMatchAddr);


/*
 * osiLocalAddr ()
 * Returns the osiSockAddr46 of the first non-loopback interface found
 * that is operational (up flag is set). If no valid address can be
 * located then return an osiSockAddr46 with the address family set to
 * unspecified (AF_UNSPEC).
 *
 * Unfortunately in EPICS 3.13 beta 11 and before the CA
 * repeater would not always allow the loopback address
 * as a local client address so current clients alternate
 * between the address of the first non-loopback interface
 * found and the loopback address when subscribing with
 * the CA repeater until all CA repeaters have been updated
 * to current code. After all CA repeaters have been restarted
 * this osi interface can be eliminated.
 */
LIBCOM_API osiSockAddr46 epicsStdCall osiLocalAddr (SOCKET socket);

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


/*
 * Support for poll(), which is not available on every system
 * Implement a very minimal wrapper that uses select()
 * We could use the native poll() on e.g. Linux or MacOs
 * and may be windows, WSAPoll
 * For the moment use the same code on all platforms
 */

#ifdef AF_INET6
# define USE_OSISOCKET_POLL_VIA_SELECT
#endif

#ifdef USE_OSISOCKET_POLL_VIA_SELECT
#include <sys/select.h>
#define POLLIN  0x1
//#define POLLOUT 0x2 not implemented yet
struct pollfd {
    int   fd;
    short events;
    short revents;
};

int osiSockPoll(struct pollfd fds[], int nfds, int timeout);
#else
# include <sys/poll.h>
# define osiSockPoll(a,b,c) poll(a,b,c)
#endif


#ifdef __cplusplus
}
#endif

#endif /* ifndef osiSockh */
