/*
 * vms specific socket include
 */

#ifndef osiSockH
#define osiSockH

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#if defined(UCX) /* GeG 09-DEC-1992 */
#       include <sys/ucx$inetdef.h>
#       include <ucx.h>
#	include <errno.h>
#else
#       include <tcp/errno.h>
#	include <sys/time.h>
#	include <sys/ioctl.h>
#       include <net/if.h>
#       include <vms/inetiodef.h>
#       include <sys/ioctl.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#if 0
int ioctl (int fd, int req, ...);
int close (int fd);
#endif
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen); 

/*
 * MULTINET defines none of these 
 */
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

unsigned long inet_addr (char *);
char * inet_ntoa (struct in_addr in);

#if 0
struct  hostent {
	char    *h_name;        /* official name of host */
	char    **h_aliases;    /* alias list */
	int     h_addrtype;     /* host address type */
	int     h_length;       /* length of address */
	char    **h_addr_list;  /* list of addresses from name server */
#define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
};
#endif
struct hostent *gethostbyaddr(char *addr, int len, int type);

void ipAddrToA (const struct sockaddr_in *pInetAddr, 
			char *pBuf, const unsigned bufSize);

#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define INVALID_SOCKET		(-1)
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


#ifdef WINTCP /* Wallangong */
	extern int      uerrno;
#	define SOCKERRNO  uerrno
#else
#	ifdef UCX
#		define SOCKERRNO errno
#	else
#		define SOCKERRNO  socket_errno
#	endif
#endif

#define MAXHOSTNAMELEN 75

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE)

#endif /*osiSockH*/

