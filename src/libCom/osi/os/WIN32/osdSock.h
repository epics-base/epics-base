


#ifdef __cplusplus
extern "C" {
#endif

#pragma warning (disable:4237)
#include <winsock.h>
 
void ipAddrToA (const struct sockaddr_in *pInetAddr, 
			char *pBuf, const unsigned bufSize);

#ifdef __cplusplus
}
#endif

#define SOCKERRNO		WSAGetLastError()
#define socket_close(S)         closesocket(S)
#define socket_ioctl(A,B,C)     ioctlsocket(A,B,(unsigned long *) C)

#define MAXHOSTNAMELEN           75
#define IPPORT_USERRESERVED      5000U
#define EWOULDBLOCK              WSAEWOULDBLOCK
#define ENOBUFS                  WSAENOBUFS
#define ECONNRESET               WSAECONNRESET
#define ETIMEDOUT                WSAETIMEDOUT
#define EADDRINUSE               WSAEADDRINUSE
#define ECONNREFUSED             WSAECONNREFUSED
#define ECONNABORTED             WSAECONNABORTED

/*
 *      Under WIN32, FD_SETSIZE is the max. number of sockets,
 *      not the max. fd value that you use in select().
 *
 *	Therefore, it is difficult to detemine if any given
 *	fd can be used with FD_SET(), FD_CLR(), and FD_ISSET().
 */
#define FD_IN_FDSET(FD) (1)

