
/*
 * Solaris specific socket include
 */

#ifndef osdSockH
#define osdSockH

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
 
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKERRNO errno
#define SOCKERRSTR(ERRNO_IN) (strerror(ERRNO_IN))
#define socket_close(S) close(S)
#define socket_ioctl(A,B,C) ioctl(A,B,C)
typedef int osiSockIoctl_t;

/* 
 * this requires emulation of SUN PRO's -D__`uname -s`_`uname -r` 
 * on 3rd party compilers
 */
#if defined ( __SunOS_5_0 ) 
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_1 )
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_2 )
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_3 ) 
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_4 ) 
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_5 ) 
    typedef int osiSocklen_t;
#elif defined ( __SunOS_5_6 ) 
    typedef int osiSocklen_t;
#else 
    typedef uint32_t osiSocklen_t;
#endif

#define DOES_NOT_ACCEPT_ZERO_LENGTH_UDP

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

#ifndef SD_BOTH
#define SD_BOTH 2
#endif

#define ifreq_size(pifreq) (sizeof(pifreq->ifr_name))

#endif /*osdSockH*/

