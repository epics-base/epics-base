


#ifdef __cplusplus
extern "C" {
#endif

#pragma warning (disable:4237)
#include <time.h>
#include <errno.h>
#include <windows.h>

#ifdef __cplusplus
}
#endif

#define SOCKERRNO			WSAGetLastError()
/*
 * I dont think that this works, but winsock does
 * not appear to provide equivalent functionality
 * here
 */
#define SOCKERRSTR (strerror(SOCKERRNO))
#define socket_close(S)		closesocket(S)
#define socket_ioctl(A,B,C)	ioctlsocket(A,B,(unsigned long *) C)

#define MAXHOSTNAMELEN		75
#define IPPORT_USERRESERVED	5000U

#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_ENOBUFS WSAENOBUFS
#define SOCK_ECONNRESET WSAECONNRESET
#define SOCK_ETIMEDOUT WSAETIMEDOUT
#define SOCK_EADDRINUSE WSAEADDRINUSE
#define SOCK_ECONNREFUSED WSAECONNREFUSED
#define SOCK_ECONNABORTED WSAECONNABORTED
#define SOCK_EINPROGRESS WSAEINPROGRESS
#define SOCK_EISCONN WSAEISCONN
#define SOCK_EALREADY WSAEALREADY
#define SOCK_EINVAL WSAEINVAL
#define SOCK_EINTR WSAEINTR
#define SOCK_EPIPE EPIPE

/*
 *	Under WIN32, FD_SETSIZE is the max. number of sockets,
 *	not the max. fd value that you use in select().
 *
 *	Therefore, it is difficult to detemine if any given
 *	fd can be used with FD_SET(), FD_CLR(), and FD_ISSET().
 */
#define FD_IN_FDSET(FD) (1)

