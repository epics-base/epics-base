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
 * vms specific socket include
 */

#ifndef osiSockH
#define osiSockH

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#if defined(MULTINET) && defined(__cplusplus)
	struct iovec;
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#if defined(UCX) /* GeG 09-DEC-1992 */
#	include <errno>
#	include <sys/ucx$inetdef.h>
#	include <ucx.h>
#	include <netdb.h>
#elif defined(MULTINET) 
#	if defined(__DECCXX)
#		define __DECC 1
#		define __DECC_VER 999999999 
#		include <tcp/errno.h>
#		include <sys/time.h>
#		undef __DECC
#		undef __DECC_VER
#	else
#		include <tcp/errno.h>
#		include <sys/time.h>
#	endif
#	if defined(__cplusplus)
		struct ifaddr;
		struct mbuf;
#	endif
#	include <sys/ioctl.h>
#	include <net/if.h>
#	include <vms/inetiodef.h>
#	include <sys/ioctl.h>
#	include <tcp/netdb.h>
#endif

/*
 * MULTINET defines none of these (if not using C++)
 */
#if defined(MULTINET) && defined(MULTINET_NO_PROTOTYPES)
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen); 
int getpeername (int socket, struct sockaddr *name, int *namelen);
int connect (int socket, struct sockaddr *name, int namelen);
int setsockopt (int socket, int level, int optname,
		char *optval, int optlen);
int sendto (int socket, const char *buf, int len,
	int flags, struct sockaddr *to, int tolen);
int select (int width, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout);

int bind (int socket, struct sockaddr *name, int namelen);
int send (int socket, const char *buf, int len, int flags);
int recv (int socket, char *buf, int len, int flags);
int getsockopt (int socket, int level, int optname,
		char *optval, int *optlen);
int recvfrom (int socket, char *buf, int len,
		int flags, struct sockaddr *from, int *fromlen);
int getsockname (int socket, struct sockaddr *name, int *namelen);
int listen (int socket, int backlog);
int shutdown (int socket, int how);
int socket (int domain, int type, int protocol);

#endif /* defined(MULTINET) && defined(MULTINET_NO_PROTOTYPES) */

#ifdef MULTINET
#	include <arpa/inet.h>
#else
	char * inet_ntoa (struct in_addr in);
	unsigned long inet_addr (const char *);
#endif

#if 0
struct  hostent {
	char    *h_name;        /* official name of host */
	char    **h_aliases;    /* alias list */
	int     h_addrtype;     /* host address type */
	int     h_length;       /* length of address */
	char    **h_addr_list;  /* list of addresses from name server */
#define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
};
struct hostent *gethostbyaddr(char *addr, int len, int type);
#endif

#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define INVALID_SOCKET		(-1)
#define INADDR_LOOPBACK ((u_long)0x7f000001)


/* 
 * (the VAXC runtime lib has its own close 
 */
#if defined(WINTCP)   /* Wallangong */
#	define socket_close(S) netclose(S)
#	define socket_ioctl(A,B,C) ioctl(A,B,C)
#endif
#if defined(UCX) /* GeG 09-DEC-1992 */
#	define socket_close(S) close(S)
#	define socket_ioctl(A,B,C) ioctl(A,B,C)
#endif
typedef int osiSockIoctl_t;

#if defined(WINTCP) /* Wallangong */
	extern int uerrno;
#	define SOCKERRNO  uerrno
#elif defined(MULTINET)
#	define SOCKERRNO socket_errno
#else
#	define SOCKERRNO errno /* UCX and others? */
#endif
    
#define MAXHOSTNAMELEN 75

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
#define SOCK_EINVAL EINVAL
#define SOCK_EINTR EINTR
#define SOCK_EPIPE EPIPE
#define SOCK_EMFILE EMFILE

/*
 * Under MULTINET FD_SETSIZE does not apply
 * (can only guess about the others)
 */
#ifdef MULTINET
#	define FD_IN_FDSET(FD) (1)
#else
#	define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE&&(FD)>=0)
#endif

#endif /*osiSockH*/

