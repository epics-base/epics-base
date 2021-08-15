/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef osdSockH
#define osdSockH

#include <time.h>
#include <errno.h>

/*
 * winsock2.h changes the structure alignment to 4 if
 * WIN32 isn't set which can be a source of confusion
 */
#ifndef WIN32
#   define WIN32
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#define SOCKERRNO WSAGetLastError()

#define socket_ioctl(A,B,C) ioctlsocket(A,B,C)
typedef u_long FAR osiSockIoctl_t;
typedef int osiSocklen_t;
typedef BOOL osiSockOptMcastLoop_t;
typedef DWORD osiSockOptMcastTTL_t;

#ifndef SHUT_RD
#   define SHUT_RD SD_RECEIVE
#endif

#ifndef SHUT_WR
#   define SHUT_WR SD_SEND
#endif

#ifndef SHUT_RDWR
#   define SHUT_RDWR SD_BOTH
#endif

#define MAXHOSTNAMELEN      75
#define IPPORT_USERRESERVED 5000U

#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_ENOBUFS WSAENOBUFS
#define SOCK_ECONNRESET WSAECONNRESET
#define SOCK_ETIMEDOUT WSAETIMEDOUT
#define SOCK_EACCES WSAEACCES
#define SOCK_EADDRINUSE WSAEADDRINUSE
#define SOCK_EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define SOCK_ECONNREFUSED WSAECONNREFUSED
#define SOCK_ECONNABORTED WSAECONNABORTED
#define SOCK_EINPROGRESS WSAEINPROGRESS
#define SOCK_EISCONN WSAEISCONN
#define SOCK_EALREADY WSAEALREADY
#define SOCK_EINVAL WSAEINVAL
#define SOCK_EINTR WSAEINTR
#define SOCK_EPIPE EPIPE
#define SOCK_EMFILE WSAEMFILE
#define SOCK_SHUTDOWN WSAESHUTDOWN
#define SOCK_ENOTSOCK WSAENOTSOCK
#define SOCK_EBADF WSAENOTSOCK
#define SOCK_EMSGSIZE WSAEMSGSIZE

/*
 *  Under WIN32, FD_SETSIZE is the max. number of sockets,
 *  not the max. fd value that you use in select().
 *
 *  Therefore, it is difficult to determine if any given
 *  fd can be used with FD_SET(), FD_CLR(), and FD_ISSET().
 */
#define FD_IN_FDSET(FD) (1)

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API unsigned epicsStdCall wsaMajorVersion ();

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /*osdSockH*/
