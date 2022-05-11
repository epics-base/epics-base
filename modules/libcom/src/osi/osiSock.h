/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*!
 * \file osiSock.h
 * \brief Platform independent socket support library API
 *
 * osiSock library contains platform independent APIs for creating sockets,
 * converting between socket address structures and human readable strings, and
 * configuring sockets.
 */

/* Author: joh 1997-07-01 */

#ifndef osiSockh
#define osiSockh

#include "libComAPI.h"
#include "osdSock.h"
#include "ellLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct sockaddr_in;
struct in_addr;

/*!
 * \brief Create a socket object
 *
 * Ask the operating system to create a socket and return its file descriptor.
 *
 * \param domain The socket domain, e.g. PF_INET
 * \param type The type of socket, e.g. SOCK_STREAM (tcp) or SOCK_DGRAM (udp)
 * \param protocol Typically unused and set to 0.
 * \return A file descriptor referring to the socket, or -1 on error.
 */
LIBCOM_API SOCKET epicsStdCall epicsSocketCreate ( 
    int domain, int type, int protocol );

/*!
 * \brief Accept a connection on a listening stream socket.
 *
 * epicsSocketAccept is used with connection-based sockets.  It takes the next
 * connection request in the queue and returns a new socket that can be used to
 * communicate with the listener.
 *
 * \param sock socket with pending connections to accept
 * \param pAddr If not NULL, this socket contains the address of the peer whose
 * connection is being accepted
 * \param[in,out] addrlen Caller must initialize this to the length of the
 * structure pointed to by \p pAddr, or set to NULL if \p pAddr is NULL.  This
 * function will update the value of this parameter to the actual size of the
 * peer address.
 * \return A new socket used for communicating with the peer just accepted, or -1 on error.
 */
LIBCOM_API int epicsStdCall epicsSocketAccept ( 
    int sock, struct sockaddr * pAddr, osiSocklen_t * addrlen );
/*!
 * \brief Close and free resources held by a SOCKET object.
 *
 * Close and free resources held by a SOCKET object.
 */
LIBCOM_API void epicsStdCall epicsSocketDestroy ( 
    SOCKET );
/*!
 * \brief Allows a socket to bind ignoring other sockets in TIME_WAIT state.
 *
 * Allows a socket to bind while other sockets hold the same address and port
 * but are in the TIME_WAIT state.  TIME_WAIT is a state where a socket has
 * closed locally, but is waiting to be closed on the remote end.  If the remote
 * end doesn't respond, this can block another socket from binding to that
 * address:port for a long time (possibly minutes).  This option allows the
 * socket to bind to an address:port combo, even if there's another socket with
 * the same address:port that's waiting for the remote side to close its
 * connection.
 *
 * See also: Linux's SO_REUSEADDR in setsockopt.
 *
 * \param s The socket to ignore other TIME_WAIT sockets when binding
 */
LIBCOM_API void epicsStdCall 
    epicsSocketEnableAddressReuseDuringTimeWaitState ( SOCKET s );
/*!
 * \brief Allows multiple sockets to use the same family, local address, and local
 * port
 *
 * Allows multiple sockets to use the same family, local address, and port.  By
 * having multiple sockets open with this setting enabled, the OS will
 * distribute the datagrams (for UDP sockets) or connection requests (for
 * TCP/stream sockets) among the sockets with this option set.  Note that all
 * sockets binding to the same address and port must have this option set.
 *
 * As an example, this option could be a way to implement a form of load
 * balancing between multiple threads or processes with sockets bound to the
 * same address:port.
 *
 * \param s The socket to put in "Fanout" mode (aka REUSEPORT mode)
 */
LIBCOM_API void epicsStdCall 
    epicsSocketEnableAddressUseForDatagramFanout ( SOCKET s );

/*!
 * \brief Enum specifying how to interrupt a blocking socket system call
 *
 * This enum specifies how to interrupt a blocking socket system call.
 * Fortunately, on most systems the combination of a shutdown of both directions
 * and/or a signal is sufficent to interrupt a blocking send, receive, or
 * connect call. For odd ball systems this is stubbed out in the osi area.
 *
 */
enum epicsSocketSystemCallInterruptMechanismQueryInfo {
    //! Calling close() required to interrupt
    esscimqi_socketCloseRequired,

    //! calling shutdown() for both read and write required to interrupt
    esscimqi_socketBothShutdownRequired,

    //! NO LONGER USED/SUPPORTED
    esscimqi_socketSigAlarmRequired
};

/*!
 * \brief Query what approach to use to interrupt blocking socket calls
 *
 * This function tells you what approach to use to interrupt calls blocked on
 * socket operations.
 *
 * \return enum representing whether to call close() or shutdown(RW) to
 * interrupt blocking socket syscalls.
 */
LIBCOM_API enum epicsSocketSystemCallInterruptMechanismQueryInfo 
        epicsSocketSystemCallInterruptMechanismQuery ();

#ifdef EPICS_PRIVATE_API
/*!
 * \brief Query unsent data in a socket's output queue
 *
 * Query unsent data in a socket's output queue.  Some systems (e.g Linux and
 * Windows 10) allow to check the amount of unsent data in the output queue.
 *
 * \param sock Socket to query for unsent data
 * \return The number of bytes of unsent data in the output queue.  Returns -1 if the
 * information is not available.
 */
LIBCOM_API int epicsSocketUnsentCount(SOCKET sock);
#endif

/*!
 * \brief Convert socket address to ASCII
 *
 * Converts a socket address to an ASCII string.  Conversion occurs in this
 * order:
 * 1. look for matching host name and typically add trailing IP port
 * 2. failing that, convert to raw ascii address (typically this is a
 *      dotted IP address with trailing port)
 * 3. failing that, writes "<Ukn Addr Type>" into pBuf
 *
 * \param paddr sockaddr structure to convert to ascii address:port
 * \param[out] pBuf Pointer to the output buffer contantaining the address:port string
 * \param bufSize Length of pBuf array
 * \return The number of character elements stored in buffer not
 * including the null termination, but always writes at least a
 * null terminator in the string (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall sockAddrToA (
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize );

/*!
 * \brief Convert IP address to ASCII
 *
 * Convert an IP address to an ASCII string.  Conversion occurs in this order:
 * 1) look for matching host name and add trailing port
 * 2) convert to raw dotted IP address with trailing port
 *
 * \param pInetAddr sockaddr_in structure to convert to address string
 * \param[out] pBuf Pointer to the char array where the address string will be stored
 * \param bufSize Length of the array to which pBuf points
 * \return The number of character elements stored in buffer not including the
 * null termination, but always writes at least a null terminator in the string
 * (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall ipAddrToA (
    const struct sockaddr_in * pInetAddr, char * pBuf, unsigned bufSize );

/*!
 * \brief Convert to raw dotted IP address with trailing port
 *
 * Convert an address to a raw dotted IP address.  Compared to sockAddrToA(),
 * this call will always convert to a raw dotted IP (e.g. 192.168.0.1) and not
 * use host names (e.g. localhost) when available
 *
 * \param paddr sockaddr structure containing the node and service info to convert to strings
 * \param[out] pBuf Pointer to a character buffer where the output string will be placed.
 * \param bufSize Size of the array pointed to by pBuf
 * \return The number of character elements stored in buffer not including the
 * null termination, but always writes at least a null terminator in the string
 * (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall sockAddrToDottedIP ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize );

/*!
 * \brief Convert to raw dotted IP address with trailing port
 *
 * Convert a sockaddr_in to a raw dotted IP address string with trailing port. Similar
 * to sockAddrToDottedIP but takes a sockaddr_in structure.
 *
 * \param paddr sockaddr_in structure containing the node and service info to convert to strings
 * \param[out] pBuf Pointer to a character buffer where the output string will be placed
 * \param bufSize Size of the array pointed to by pBuf
 * \return The number of character elements stored in buffer not including the
 * null termination, but always writes at least a null terminator in the string
 * (if bufSize >= 1)
 */
LIBCOM_API unsigned epicsStdCall ipAddrToDottedIP ( 
    const struct sockaddr_in * paddr, char * pBuf, unsigned bufSize );

/*!
 * \brief Convert inet address to a host name string
 *
 * Convert an inet address to a host name string. There are many OS specific
 * implementation stubs for this routine
 *
 * \param pAddr sockaddr_in structure containing the node and service info to convert to strings
 * \param[out] pBuf Pointer to a character buffer where the output string will be placed
 * \param bufSize Size of the array pointed to by pBuf
 * \return the number of character elements stored in buffer not including the
 * null termination. This will be zero if a matching host name cant be found.
 */
LIBCOM_API unsigned epicsStdCall ipAddrToHostName (
    const struct in_addr * pAddr, char * pBuf, unsigned bufSize );

/*!
 * \brief Attempt to convert ASCII string to an IP address
 *
 * Attempt to convert ASCII string to an IP address. Conversion occurs this
 * order:
 * 1. look for traditional doted ip with optional port
 * 2. look for raw number form of ip address with optional port
 * 3. look for valid host name with optional port
 *
 * \param pAddrString IP address string to convert to a sockaddr_in (e.g. "192.168.0.1:80")
 * \param defaultPort The default port (service) to use if not specified in pAddrString
 * \param[out] pIP Pointer to a sockaddr_in where the converted address info will be stored
 * \return negative value on error, 0 on success.
 */
LIBCOM_API int epicsStdCall aToIPAddr
    ( const char * pAddrString, unsigned short defaultPort, struct sockaddr_in * pIP);

/*!
 * \brief Attempt to convert ASCII host name string with optional port to an IP address
 *
 * Convert an in_addr struct to a human readable hostname.
 *
 * \param pHostName ASCII host name to convert
 * \param[out] pIPA Pointer to the in_addr structure where the hostname information will be stored
 * \return 0 on success, negative on error.
 */
LIBCOM_API int epicsStdCall hostToIPAddr 
    (const char *pHostName, struct in_addr *pIPA);
/*!
 * \brief attach to BSD socket library

 * Attach to the BSD socket library in preparation for using sockets.  If the OS
 * does not require attaching to the socket library, this function does nothing.
 *
 * \return true on success, false on error.
 */
LIBCOM_API int epicsStdCall osiSockAttach (void); /* returns T if success, else F */

/*!
 * \brief release BSD socket library
 *
 * Release the BSD socket library if the OS requires it.  For platforms that do
 * not require attaching and releasing to use the socket library, this function
 * does nothing.
 */
LIBCOM_API void epicsStdCall osiSockRelease (void);

/*!
 * \brief convert socket error numbers to a string
 *
 * There are several system functions which set errno on failure.  This function
 * converts that error number into a string describing the error.
 *
 * \param[out] pBuf Pointer to char array where the error string will be stored
 * \param bufSize Length of the array pointed to by pBuf
 * \param error The error number to describe in string form
 */
LIBCOM_API void epicsSocketConvertErrorToString (
        char * pBuf, unsigned bufSize, int error );

/*!
 * \brief Convert the currently set errno to a string
 *
 * errno is a global integer that gets set when certain functions return an
 * error.  This function converts that error value into a string describing the
 * error.
 *
 * \param[out] pBuf Pointer to char array where the error string will be stored
 * \param bufSize Length of the array pointed to by pBuf
 */
LIBCOM_API void epicsSocketConvertErrnoToString (
        char * pBuf, unsigned bufSize );

/*!
 * \brief Union to switch between sockaddr_in and sockaddr
 *
 * A union to switch between sockaddr_in and sockaddr. Several socket related
 * calls require casting between a sockaddr and a sockaddr_in.  This union makes
 * that casting between the two more convenient.
 */
typedef union osiSockAddr {
    struct sockaddr_in  ia;
    struct sockaddr     sa;
} osiSockAddr;

/*!
 * \brief Stores a list of socket addresses
 *
 * Container to store a list of socket addresses
 */
typedef struct osiSockAddrNode {
    ELLNODE node;
    osiSockAddr addr;
} osiSockAddrNode;

/*!
 * \brief Compares two osiSockAddrs
 *
 * Compares two osiSockAddrs
 *
 * \return true if addresses are identical, false otherwise.
 */
LIBCOM_API int epicsStdCall sockAddrAreIdentical 
    ( const osiSockAddr * plhs, const osiSockAddr * prhs );

/*!
 * \brief Add available broadcast addresses to a list
 *
 *  Add available broadcast addresses to a list. This routine is provided with
 *  the address of an ELLLIST, a socket, and a match address. When the routine
 *  returns there will be one additional entry in the list for each network
 *  interface found that is up and isn't a loop back interface (match addr is
 *  INADDR_ANY), or only the interfaces that match the specified addresses
 *  (match addr is other than INADDR_ANY). If the interface supports
 *  broadcasting then add its broadcast address to the list. If the interface is
 *  a point to point link then add the destination address of the point to point
 *  link to the list.
 *
 *  Any mutex locking required to protect pList is applied externally.
 *
 * Example
 * =======
 *  \code{.cpp}
 *  ELLLIST ifaces = ELLLIST_INIT;
 *  ELLNODE * cur;
 *  SOCKET dummy = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
 *  osiSockAddr match;
 *
 *  memset(&match, 0, sizeof(match));
 *  match.ia.sin_family = AF_INET;
 *  match.ia.sin_addr.s_addr = htonl(INADDR_ANY);
 *
 *  osiSockDiscoverBroadcastAddresses(&ifaces, dummy, &match);
 *  for (cur = ellFirst(&ifaces); cur; cur = ellNext(cur)) {
 *      char name[64];
 *      osiSockAddrNode * n = CONTAINER(cur, osiSockAddrNode, node);
 *      node->addr.ia.sin_port = htons(5555);
 *      sockAddrToDottedIP(&node->addr.sa, name, sizeof(name);
 *      fprintf(stderr, "Node is %s", name);
 *  }
    ellFree(&ifaces);
    epicsSocketDestroy(dummy);
 *  \endcode
 *
 *  \param[out] pList Pointer to a list where network interfaces will be added as new osiSockAddrNode's
 *  \param socket (May be unused)
 *  \param pMatchAddr Only add addresses that match this address. Use
 *  match.ia.sin_addr.s_addr = htonl(INADDR_ANY) to match any address.
 *  \return The broadcast addresses of each network interface found.
 */
LIBCOM_API void epicsStdCall osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr);

/*!
 * Returns the osiSockAddr of the first non-loopback interface found that is
 * operational (up flag is set). If no valid address can be located then return
 * an osiSockAddr with the address family set to unspecified (AF_UNSPEC).
 *
 * Unfortunately in EPICS 3.13 beta 11 and before the CA repeater would not
 * always allow the loopback address as a local client address so current
 * clients alternate between the address of the first non-loopback interface
 * found and the loopback address when subscribing with the CA repeater until
 * all CA repeaters have been updated to current code.
 *
 * \note After all CA repeaters have been restarted this osi interface can be
 * eliminated.
 */
LIBCOM_API osiSockAddr epicsStdCall osiLocalAddr (SOCKET socket);

#ifdef __cplusplus
}
#endif

#endif /* ifndef osiSockh */
