/*
 * $Id
 *
 * Author Jeff Hill
 *
 * BSD prototypes missing from SUNOS4, MULTINET and
 * perhaps other environments
 *
 */

#ifndef bsdProtInc
#define bsdProtInc

#if defined (SUNOS4) || defined(MULTINET) 

#ifdef __cplusplus
extern "C" {
#endif

#include <epicsTypes.h>


int ioctl (int fd, int req, ...);
int socket_ioctl (int fd, int req, ...);
int socket_close (int fd);
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen);
int ftruncate (int fd, long length);

#ifndef NOBSDNETPROTO

/* 
 * sun's CC defines at least a few of these under sunos4 
 */
#if !defined(__SUNPRO_CC) && defined(SUNOS4)

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
 * (__SUNPRO_CC supplies this file but g++ does not supply an ansi protottype)
 */
unsigned long inet_addr (char *);
char * inet_ntoa (struct in_addr in);

/*
 * from /usr/include/netdb.h
 * (which under sunos4 does not include arguments for C++)
 * (__SUNPRO_CC supplies this file but g++ does not)
 */
struct hostent *gethostbyaddr(char *addr, int len, int type);

#endif /* !defined(__SUNPRO_CC) && defined(SUNOS4) */

#ifdef __SUNPRO_CC 
#include <arpa/inet.h>
#endif

#if !defined (htonl) || defined(MULTINET)
epicsInt32 htonl (epicsInt32);
epicsInt16 htons (epicsInt16);
epicsInt32 ntohl (epicsInt32);
epicsInt16 ntohs (epicsInt16);
#endif

#endif /*NOBSDNETPROTO*/

#ifdef __cplusplus
}
#endif

#endif /* defined(SUNOS4) */

#endif /* bsdProtInc */

