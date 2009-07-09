/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


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
