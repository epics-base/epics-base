

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#if defined(WINTCP)   /* Wallangong */
#	define socket_close(S) netclose(S)
#	define socket_ioctl(A,B,C) ioctl(A,B,C)
#       include         <net/if.h>
#       include         <vms/inetiodef.h>
#       include         <sys/ioctl.h>
#	include 	<tcp/errno.h>
#	define SOCKERRNO 	uerrno
#elif defined(UCX)    /* GeG 09-DEC-1992 */
#       include         <sys/ucx$inetdef.h>
#       include         <ucx.h>
#	define socket_close(S) close(S)
#      	define socket_ioctl(A,B,C) ioctl(A,B,C)
#	include		<sys/errno.h>
#	define SOCKERRNO errno
#else /* MULTINET */
#       include         <net/if.h>
#       include         <vms/inetiodef.h>
#       include         <sys/ioctl.h>
#	include 	<tcp/errno.h>
#	define SOCKERRNO socket_errno
#endif

/*
 * NOOP out task watch dog for now
 */
#define taskwdInsert(A,B,C)
#define taskwdRemove(A)

#ifndef LOCAL
#define LOCAL static
#endif /*LOCAL*/

