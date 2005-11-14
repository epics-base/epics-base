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
 * sunos 4 specific socket include
 */

#ifndef osdSockH
#define osdSockH

/*
 * done in two ifdef steps so that we will remain compatible with
 * traditional C
 */

#ifdef __cplusplus
extern "C" {
#define OSISOCK_ANSI_FUNC_PROTO
#endif

#ifdef __STDC__
#define OSISOCK_ANSI_FUNC_PROTO
#endif

#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>

#ifdef OSISOCK_ANSI_FUNC_PROTO
int ioctl (int fd, int req, ...);
int close (int fd);
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen); 
#endif

/*
 * sun's CC defines at least a few of these under sunos4
 * ( but acc does not !? )
 */
#if defined(__SUNPRO_CC) 
#	include <arpa/inet.h>
#	include <netdb.h>
#else
#	ifdef OSISOCK_ANSI_FUNC_PROTO
		int listen (int socket, int backlog);
		int shutdown (int socket, int how);
		int getpeername (int socket, struct sockaddr *name, int *namelen);
		int connect (int socket, struct sockaddr *name, int namelen);
		int setsockopt (int socket, int level, int optname,
				char *optval, int optlen);
		void bzero (char *b, int length);
		int sendto (int socket, const char *buf, int len,
			int flags, struct sockaddr *to, int tolen);
		int select (int width, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout);

		int bind (int socket, struct sockaddr *name, int namelen);
		int send (int socket, const char *buf, int len, int flags);
		int recv (int socket, char *buf, int len, int flags);
		int getsockopt (int socket, int level, int optname,
				char *optval, int *optlen);
		int socket (int domain, int type, int protocol);
		int recvfrom (int socket, char *buf, int len,
			int flags, struct sockaddr *from, int *fromlen);
		int getsockname (int socket, struct sockaddr *name, int *namelen);

		/*
		 * from /usr/include/arpa/inet.h
		 * (which under sunos4 does not include arguments for C++)
		 * (__SUNPRO_CC supplies this file but g++ does not supply 
		 * an ansi protottype)
		 */
		unsigned long inet_addr (const char *);
		char * inet_ntoa (struct in_addr in);

		/*
		 * from /usr/include/netdb.h
		 * (which under sunos4 does not include arguments for C++)
		 * (__SUNPRO_CC supplies an updated version of this file but g++ does not)
		 */
		struct  hostent {
			char    *h_name;        /* official name of host */
			char    **h_aliases;    /* alias list */
			int     h_addrtype;     /* host address type */
			int     h_length;       /* length of address */
			char    **h_addr_list;  /* list of addresses from name server */
	#	define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
		};
		struct hostent *gethostbyaddr(char *addr, int len, int type);
		struct hostent *gethostnyname(const char *name);
#	endif /* ifdef OSISOCK_ANSI_FUNC_PROTO */
#endif /* !defined(__SUNPRO_CC) */

#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define INVALID_SOCKET		(-1)
#define SOCKERRNO               errno
#define socket_ioctl(A,B,C)     ioctl(A,B,C)
typedef int osiSockIoctl_t;
typedef int osiSocklen_t;

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE&&(FD)>=0)

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
#define SOCK_SHUTDOWN ESHUTDOWN
#define SOCK_ENOTSOCK ENOTSOCK
#define SOCK_EBADF EBADF

#ifndef INADDR_NONE
#   define INADDR_NONE (0xffffffff)
#endif 

#define ifreq_size(pifreq) (sizeof(pifreq->ifr_name))

#endif /*osdSockH*/

