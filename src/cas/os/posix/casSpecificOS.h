/*
 * $Id$
 * Some of this isnt posix - its BSD
 *
 * $Log$
 */

#ifndef includeCasSpecificOSH
#define includeCasSpecificOSH

#include <singleThread.h>

#include <unistd.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <bsdProto.h>

#ifdef __cplusplus
}
#endif


typedef int                     SOCKET;
#define SOCKERRNO               errno
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)

#define ntohf(A) (A)
#define ntohd(A) (A)
#define htonf(A) (A)
#define htond(A) (A)

#endif /* ifndef includeCasSpecificOSH (no new code below this line) */

