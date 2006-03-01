/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Interix specific socket include
 */

#ifndef osdSockH
#define osdSockH


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h> /* gethosthname,close() and others */

#if 0
#include <sys/types.h>
#include <sys/param.h> /* for MAXHOSTNAMELEN */
#endif

#ifdef __cplusplus
}
#endif
 
typedef int SOCKET;
#if defined ( _SOCKLEN_T )
    typedef uint32_t osiSocklen_t;
#else 
    typedef int osiSocklen_t;
#endif
#define SOCKERRNO errno
#define INVALID_SOCKET (-1)
typedef int osiSockIoctl_t;
#define socket_ioctl(A,B,C) ioctl(A,B,C)

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE&&(FD)>=0)

#define DOES_NOT_ACCEPT_ZERO_LENGTH_UDP

#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_ENOBUFS ENOBUFS
#define SOCK_ECONNRESET ECONNRESET
#define SOCK_ETIMEDOUT ETIMEDOUT
#define SOCK_EADDRINUSE EADDRINUSE
#define SOCK_ECONNREFUSED ECONNREFUSED
#define SOCK_ECONNABORTED ECONNABORTED
#define SOCK_EINPROGRESS EINPROGRESS
#define SOCK_EISCONN EISCONN
#define SOCK_EALREADY EALREADY
#define SOCK_EINTR EINTR
#define SOCK_EINVAL EINVAL
#define SOCK_EPIPE EPIPE
#define SOCK_EMFILE EMFILE
#define SOCK_SHUTDOWN ESHUTDOWN
#define SOCK_ENOTSOCK ENOTSOCK
#define SOCK_EBADF EBADF


#define	IFF_UP		0x1		/* interface is up */
#define	IFF_BROADCAST	0x2		/* broadcast address valid */
#define	IFF_LOOPBACK	0x8		/* is a loopback net */
#define IFF_POINTOPOINT 0x10            /* interface is point to point */

/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
#define	IFNAMSIZ	16
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		caddr_t	ifru_data;
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
};

/* Structure used in SIOCGIFCONF request.
 * Used to retrieve interface configuration
 * for machine (useful for programs which
 * must know all networks accessible).
 */
struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		caddr_t	ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

#ifndef NBBY
#define NBBY 8
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE    256
#endif

#define NFDBITS (sizeof (fd_mask) * NBBY )       /* bits per mask */
#ifndef howmany
#define howmany(x, y)  (((x)+((y)-1))/(y))
#endif

#define TWOPOWER32 4294967296.0
#define TWOPOWER31 2147483648.0
#define UNIX_EPOCH_AS_MJD 40587.0

#define INVALID_SOCKET		(-1)
#define INADDR_LOOPBACK ((u_long)0x7f000001)

#ifndef INADDR_NONE
#   define INADDR_NONE (0xffffffff)
#endif 


#endif /*osdSockH*/

