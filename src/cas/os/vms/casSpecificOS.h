/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <singleThread.h>

#include <sys/types.h>

#if defined (MULTINET)
#	include "multinet_root:[multinet.include]errno.h"
#elif defined (WINTCP)
#	include <tcp/errno.h>
#else
#	include <errno.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if defined(UCX) && 0
#       include <sys/ucx$inetdef.h>
#       include <ucx.h>
#else
#	include <sys/ioctl.h>
#endif

#include <bsdProto.h>

typedef int                     SOCKET;

#if defined(WINTCP)
	extern int		uerrno;
#	define SOCKERRNO        uerrno
#elif defined(MULTINET)
#	define SOCKERRNO	socket_errno
#else
#	define SOCKERRNO	errno
#endif

#if defined (UCX)
#	define socket_close(S)         close (S)
#	define socket_ioctl(A,B,C)     ioctl (A,B,C)
#elif defined (WINTCP)
#	define socket_close(S)         netclose (S)
#	define socket_ioctl(A,B,C)     ioctl (A,B,C)
#endif /* UCX */

#include <mitfp.h>

