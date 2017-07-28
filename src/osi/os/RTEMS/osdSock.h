/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * RTEMS osdSock.h
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */
#ifndef osdSockH
#define osdSockH

#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

int select(int  n,  fd_set  *readfds,  fd_set  *writefds, fd_set *exceptfds, struct timeval *timeout);

#ifdef __cplusplus
}
#endif

typedef int                     SOCKET;
#define INVALID_SOCKET          (-1)
#define SOCKERRNO               errno
#define socket_ioctl(A,B,C)     ioctl(A,B,C)
typedef int osiSockIoctl_t;
typedef socklen_t osiSocklen_t;

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE)

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
#define SOCK_SHUTDOWN EPIPE
#define SOCK_ENOTSOCK ENOTSOCK
#define SOCK_EBADF EBADF

#define bzero(p,n) memset(p,0,n)
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef INADDR_LOOPBACK
#define       INADDR_LOOPBACK         (u_long)0x7F000001
#endif

#ifndef INADDR_NONE
#   define INADDR_NONE (0xffffffff)
#endif

/*
 * For shutdown()
 */
#ifndef SHUT_RD
#   define SHUT_RD 0
#endif

#ifndef SHUT_WR
#   define SHUT_WR 1
#endif

#ifndef SHUT_RDWR
#   define SHUT_RDWR 2
#endif

/*
 * Ensure that we get the right network code in default/osdNetIntf.c.
 */
#define ifreq_size(pifreq) (pifreq->ifr_addr.sa_len + sizeof(pifreq->ifr_name))

#endif /*osdSockH*/
