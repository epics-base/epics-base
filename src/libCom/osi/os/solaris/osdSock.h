
/*
 * Solaris specifif socket include
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
 
#ifdef __cplusplus
}
#endif
 
typedef int                     SOCKET;
#define SOCKERRNO               errno
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)


