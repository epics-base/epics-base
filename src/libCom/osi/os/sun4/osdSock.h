/*
 * sunos 4 specific socket include
 */

#ifndef osiSockH
#define osiSockH

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
 
int ioctl (int fd, int req, ...);
int close (int fd);
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen); 

void ipAddrToA (const struct sockaddr_in *pInetAddr, 
			char *pBuf, const unsigned bufSize);

/*
 * sun's CC defines at least a few of these under sunos4
 * ( but acc does not !? )
 */
#if defined(__SUNPRO_CC) 
#	include <arpa/inet.h>
#	include <netdb.h>
#else
	int listen (int socket, int backlog);
	int accept (int socket, struct sockaddr *addr, int *addrlen);
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
	unsigned long inet_addr (char *);
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

#endif /* !defined(__SUNPRO_CC) */

#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define INVALID_SOCKET		(-1)
#define SOCKERRNO               errno
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)


#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE)

#endif /*osiSockH*/

