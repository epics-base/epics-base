/*
 * cygwin32 specific include
 */


#ifndef osiSockH
#define osiSockH

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#include <sys/types.h>
#include <sys/param.h> /* for MAXHOSTNAMELEN */
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h> /* close() and others */
 

/*
 * as far as I can tell there is no header file for these
 */
int gethostname(char *name, int namelen);
int sethostname(char *name, int namelen);

#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define INVALID_SOCKET		(-1)
#define SOCKERRNO               errno
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)

#define FD_IN_FDSET(FD) ((FD)<FD_SETSIZE)

/*
 * Windows Sockets errors redefined as regular Berkeley error constants
 */
#define ETIMEDOUT		WSAETIMEDOUT

/*
 * All Windows Sockets error constants are biased by WSABASEERR from
 * the "normal"
 */
#define WSABASEERR		10000
#define WSAETIMEDOUT		(WSABASEERR+60)


#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)

#define _IOWR(x, y, t) \
        (IOC_INOUT|((((int)sizeof (t))&IOCPARM_MASK)<<16)|(x<<8)|y)
 
/* Used by ca/if_depends.c */
#define    SIOCGIFDSTADDR  _IOWR('i', 15, struct ifreq)    /* get p-p address */
#define    SIOCGIFADDR     _IOWR('i', 13, struct ifreq)    /* get ifnet address */

/* Used by ca/if_depends.c db/drvTS.c dbtools/BSlib.c */
#define    SIOCGIFBRDADDR  _IOWR('i', 23, struct ifreq)    /* get broadcast addr */


/* Used by ca/if_depends.c ca/ucx.h */
#define    IFF_POINTOPOINT 0x10

/* Used by ca/iocinf.c  */
#define SO_RCVBUF          0x1002   /* receive buffer size */
#define WSAEISCONN         (WSABASEERR+56)
#define WSAEINPROGRESS     (WSABASEERR+36)
#define WSAEALREADY        (WSABASEERR+37)
#define EISCONN            WSAEISCONN
#define EINPROGRESS        WSAEINPROGRESS
#define EALREADY           WSAEALREADY


#endif /*osiSockH*/

