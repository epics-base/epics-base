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
 * Solaris specific socket include
 */

#ifndef osdSockH
#define osdSockH

#include <errno.h>

#include <sys/types.h>
#include <sys/param.h> /* for MAXHOSTNAMELEN */
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h> /* close() and others */


typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKERRNO errno
#define socket_ioctl(A,B,C) ioctl(A,B,C)
typedef int osiSockIoctl_t;

#if SOLARIS > 6 || defined ( _SOCKLEN_T )
    typedef uint32_t osiSocklen_t;
#else
    typedef int osiSocklen_t;
#endif
typedef char osiSockOptMcastLoop_t;
typedef unsigned char osiSockOptMcastTTL_t;

#define DOES_NOT_ACCEPT_ZERO_LENGTH_UDP

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE&&(FD)>=0)

#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_ENOBUFS ENOBUFS
#define SOCK_ECONNRESET ECONNRESET
#define SOCK_ETIMEDOUT ETIMEDOUT
#define SOCK_EACCES EACCES
#define SOCK_EADDRINUSE EADDRINUSE
#define SOCK_EADDRNOTAVAIL EADDRNOTAVAIL
#define SOCK_ECONNREFUSED ECONNREFUSED
#define SOCK_ECONNABORTED ECONNABORTED
#define SOCK_EINPROGRESS EINPROGRESS
#define SOCK_EISCONN EISCONN
#define SOCK_EALREADY EALREADY
#define SOCK_EINVAL EINVAL
#define SOCK_EINTR EINTR
#define SOCK_EPIPE EPIPE
#define SOCK_EMFILE EMFILE
#define SOCK_SHUTDOWN ESHUTDOWN
#define SOCK_ENOTSOCK ENOTSOCK
#define SOCK_EBADF EBADF
#define SOCK_EMSGSIZE EMSGSIZE

#ifndef SHUT_RD
#   define SHUT_RD 0
#endif

#ifndef SHUT_WR
#   define SHUT_WR 1
#endif

#ifndef SHUT_RDWR
#   define SHUT_RDWR 2
#endif

#ifndef INADDR_NONE
#   define INADDR_NONE (0xffffffff)
#endif

#define ifreq_size(pifreq) (sizeof(pifreq->ifr_name))

#endif /*osdSockH*/
