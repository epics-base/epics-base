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

#ifdef __cplusplus
extern "C" {
#endif

#if defined (SUNOS4) || defined(MULTINET)

#include <epicsTypes.h>


int ioctl (int fd, int req, ...);
int socket_ioctl (int fd, int req, ...);
int socket_close (int fd);
int gettimeofday (struct timeval *tp, struct timezone *tzp);
int gethostname (char *name, int namelen);
int ftruncate (int fd, long length);

#ifndef NOBSDNETPROTO

#ifndef __cplusplus
void bzero (char *b, int length);
int select (int width, fd_set *readfds, fd_set *writefds, 
	fd_set *exceptfds, struct timeval *timeout);
int setsockopt (int socket, int level, int optname, 
		char *optval, int optlen);
int send (int socket, char *buf, int len, int flags);
int sendto (int socket, char *buf, int len,
        int flags, struct sockaddr *to, int tolen);
int connect (int socket, struct sockaddr *name, int namelen);
unsigned long inet_addr (char *);
#endif

int socket (int domain, int type, int protocol);
int bind (int socket, struct sockaddr *name, int namelen);
int listen (int socket, int backlog);
int accept (int socket, struct sockaddr *addr, int *addrlen);
int shutdown (int socket, int how);
int recv (int socket, char *buf, int len, int flags);
int recvfrom (int socket, char *buf, int len,
        int flags, struct sockaddr *from, int *fromlen);
int getpeername (int socket, struct sockaddr *name, int *namelen);
int getsockname (int socket, struct sockaddr *name, int *namelen);
char * inet_ntoa (struct in_addr in);

#if !defined (htonl) || defined(MULTINET)
epicsInt32 htonl (epicsInt32);
epicsInt16 htons (epicsInt16);
epicsInt32 ntohl (epicsInt32);
epicsInt16 ntohs (epicsInt16);
#endif

#endif /*BSDNETPROTO*/

#endif /* defined(SUNOS4) */

#ifdef __cplusplus
}
#endif

#endif /* bsdProtInc */

