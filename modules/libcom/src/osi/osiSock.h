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
    osiSockAddr46 addr;
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
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr);

/*
 *  osiSockBroadcastMulticastAddresses46 ()
 *  Returns the broadcast addresses of each IPv4 interface found.
 *  Returns the multicast addresses of each IPv6 interface found.
 *
 *  pMatchAddr->ia.sin_family = selects if IPv4, IPv6 or both are returned:
 *  AF_INET:      IPv4 broadcast (same as osiSockDiscoverBroadcastAddresses)
 *  AF_INET6:     IPv6 multicast
 *  AF_UNSPEC:    both IPv4 broadcast and IPv6 multicast
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
LIBCOM_API void epicsStdCall osiSockBroadcastMulticastAddresses46
     (ELLLIST *pList, SOCKET socket, const osiSockAddr46 *pMatchAddr46);


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

#ifdef __cplusplus
}
#endif

#endif /* ifndef osiSockh */
