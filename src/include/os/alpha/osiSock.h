
/*
 * alpha specific socket include
 */

#ifndef osiSockH
#define osiSockH

#ifdef __cplusplus
extern "C" {
struct ifafilt;
#endif

#include <errno.h>

#include <sys/types.h>
#include <sys/param.h> /* for MAXHOSTNAMELEN */
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
/* #include <sys/filio.h> */
/* #include <sys/sockio.h> */
#include <netinet/in.h>
#include <netinet/tcp.h>
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
#define SOCKERRSTR		  (strerror(errno))
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)
typedef int osiSockIoctl_t;
typedef int osiSocklen_t;

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

#define ifreq_size(pifreq) sizeof(*pifreq)

#endif /*osiSockH*/

