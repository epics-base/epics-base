/*
 *	O S _ D E P E N . H
 *
 *
 *	OS dependent stuff for channel access
 *	Author Jeffrey O. Hill
 *
 *	History
 *	.01 joh	110190	Moved to this file
 *	.02 joh 082891	on UNIX copy in a new delay into the timeval 
 *			arg to select each call in TCPDELAY in case 
 *			the system modifies my timeval structure.
 *			Also droped the TCPDELAY to 50 mS
 *	.03 joh	090391	Updated for V5 vxWorks
 *	.04 joh 092491	changed order of ops on LOCALTICKS
 *	.05 joh	092691	added NELEMENTS()
 *	.06 joh 111991	added EVENTLOCKTEST
 *	.07 joh 120291	added declaration of taskIdCurrent for
 *			compiling with V4 vxWorks 
 *	.08 joh	062692	took out printf to logMsg MACRO
 *	.09 joh 101692	dont quit if select() is interrupted in
 *			the UNIX version of TCPDELAY
 *	.10 joh	112092	removed the requirement that VMS locking
 *			pairs reside at the same C bracket level
 *      .11 GeG 120992	support VMS/UCX
 *      .12 CJM 130794  define MYERRNO properly for UCX
 *	.13 CJM 311094  mods to support DEC C compiler
 *
 */

#ifndef INCos_depenh
#define INCos_depenh

static char *os_depenhSccsId = "$Id$";

/*
 * errno.h is ANSI however we
 * include it here because of differences
 * between error code sets provided by
 * each socket library
 */
#ifdef UNIX
#	include <unistd.h>
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/time.h>
#	include <sys/ioctl.h>
#	include <sys/param.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <net/if.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <sys/filio.h>
#	include <sys/sockio.h>
#	define CA_OS_CONFIGURED
#endif

#ifdef vxWorks
#	include <vxWorks.h>
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <net/if.h>

#	include <systime.h>
#	include <ioLib.h>
#	include <tickLib.h>
#	include <taskHookLib.h>
#	include <selectLib.h>
#	include <sockLib.h>
#       include <errnoLib.h>
#       include <sysLib.h>
#       include <taskVarLib.h>
#       include <hostLib.h>
#       include <logLib.h>
#       include <usrLib.h>
#       include <dbgLib.h>
#	include <inetLib.h>
#	include <taskLib.h>

#	include <task_params.h>
#	include <taskwd.h>
#	include <fast_lock.h>

/*
 * logistical problems prevent including this file
 */
#if 0
#define caClient
#include                <dbEvent.h>
#endif
#	define CA_OS_CONFIGURED
#endif

#ifdef VMS
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#if !defined(UCX) 
#	include <sys/time.h>
#	include <tcp/errno.h>
#else
#       include <errno>
#endif
#	include <ssdef>
#	include <stsdef>
#	include <iodef.h>
#	include <psldef.h>
#       include <prcdef.h>
#       include <descrip.h>
#	define MAXHOSTNAMELEN 75
#ifdef UCX /* GeG 09-DEC-1992 */
#       include <sys/ucx$inetdef.h>
#       include <ucx.h>
#else
#	include	<net/if.h>
#       include <vms/inetiodef.h>
#       include <sys/ioctl.h>
#endif
#	define CA_OS_CONFIGURED
#endif /*VMS*/

#ifdef WIN32
#	include <errno.h>
#	include <time.h>
#	include <windows.h>
#	include <winsock.h>
#	define CA_OS_CONFIGURED
#endif /*WIN32*/

#ifndef CA_OS_CONFIGURED
#error Please define one of vxWorks, UNIX, VMS, or WIN32 
#endif

/*
 * Here are the definitions for architecture dependent byte ordering 
 * and floating point format
 */
#if defined(VAX) 
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif defined(_X86_)
#	define CA_FLOAT_IEEE
#	define CA_LITTLE_ENDIAN
#elif (defined(__ALPHA) && defined(VMS) || defined(__alpha)) && defined(VMS)
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif (defined(__ALPHA) && defined(UNIX) || defined(__alpha)) && defined(UNIX)
#	define CA_FLOAT_IEEE
#	define CA_LITTLE_ENDIAN
#else
#	define CA_FLOAT_IEEE
#	define CA_BIG_ENDIAN
#endif

/*
 * some architecture sanity checks
 */
#if defined(CA_BIG_ENDIAN) && defined(CA_LITTLE_ENDIAN)
#error defined(CA_BIG_ENDIAN) && defined(CA_LITTLE_ENDIAN)
#endif
#if !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#error !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#endif
#if defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#error defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#endif
#if !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT)
#error !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT) 
#endif

/*
 * CONVERSION_REQUIRED is set if either the byte order
 * or the floating point does not match
 */
#if !defined(CA_FLOAT_IEEE) || !defined(CA_BIG_ENDIAN)
#define CONVERSION_REQUIRED
#endif

#ifndef NULL
#define NULL            0
#endif  

#ifndef FALSE
#define FALSE           0
#endif  

#ifndef TRUE
#define TRUE            1
#endif  

#ifndef OK
#define OK            0
#endif  

#ifndef ERROR
#define ERROR            (-1)
#endif  

#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif

#ifndef LOCAL 
#define LOCAL static
#endif

/* delay for when a poll is used	 				*/
/*	NOTE: DELAYTICKS must be less than TICKSPERSEC	*/
#define DELAYTICKS	50L		/* (adjust units below) */
#define TICKSPERSEC	1000L		/* mili sec per sec	*/

/*
 * BSD prototypes missing from SUNOS4, MULTINET and 
 * perhaps other environments
 */
#include <epicsTypes.h>
#include <bsdProto.h>

/*
 * order of ops is important here
 * 
 * NOTE: large OS dependent SYFREQ might cause an overflow 
 */
#define LOCALTICKS	((SYSFREQ*DELAYTICKS)/TICKSPERSEC)


#if defined(vxWorks)
#	define VXTASKIDNONE	0
#  	define LOCK 		semTake(client_lock, WAIT_FOREVER);
#  	define UNLOCK  	semGive(client_lock);
#	define LOCKEVENTS \
	{semTake(event_lock, WAIT_FOREVER); event_tid=(int)taskIdCurrent;}
#  	define UNLOCKEVENTS \
	{event_tid=VXTASKIDNONE; semGive(event_lock);}
#	define EVENTLOCKTEST \
(((int)taskIdCurrent)==event_tid || ca_static->recv_tid == (int)taskIdCurrent)
#	define VXTHISTASKID 	taskIdSelf()
#	define abort() 		taskSuspend(VXTHISTASKID)
#   	define socket_close(S) close(S)
	/* vxWorks still has a brain dead func proto for ioctl */
#   	define socket_ioctl(A,B,C) ioctl(A,B,(int)C) 
# 	define MYERRNO	(errnoGet()&0xffff)
#  	define POST_IO_EV semGive(io_done_sem)
# 	define SYSFREQ		((long) sysClkRateGet())  /* usually 60 Hz */
# 	define time(A) 		(tickGet()/SYSFREQ)
	typedef int SOCKET;
#	define INVALID_SOCKET (-1)
#endif

#if defined(UNIX)
#  	define LOCK
#  	define UNLOCK  
#  	define LOCKEVENTS
#  	define UNLOCKEVENTS
#	define EVENTLOCKTEST	(post_msg_active)
#   	define socket_close(S) close(S)
#   	define socket_ioctl(A,B,C) ioctl(A,B,C) 
# 	define MYERRNO	errno
#  	define POST_IO_EV 
#	define SYSFREQ		1000000L	/* 1 MHz	*/
	typedef int SOCKET;
#	define INVALID_SOCKET (-1)
#endif

#if defined(VMS)
#  if defined(WINTCP)	/* Wallangong */
	/* (the VAXC runtime lib has its own close */
# 		define socket_close(S) netclose(S)
# 		define socket_ioctl(A,B,C) ioctl(A,B,C) 
#  endif
#  if defined(UCX)				/* GeG 09-DEC-1992 */
# 		define socket_close(S) close(S)
# 		define socket_ioctl(A,B,C) ioctl(A,B,C) 
#  endif
#	ifdef WINTCP
  		extern int	uerrno;	
# 		define MYERRNO	uerrno
#	else
#           ifdef UCX
#               define MYERRNO errno
#           else
# 		define MYERRNO	socket_errno
#           endif
#	endif
#  	define POST_IO_EV 
# 	define SYSFREQ		10000000L	/* 10 MHz	*/
#	define LOCK
#	define UNLOCK
#  	define LOCKEVENTS
#  	define UNLOCKEVENTS
#	define EVENTLOCKTEST	(post_msg_active)
	typedef int SOCKET;
#	define INVALID_SOCKET (-1)
#endif

#ifdef WIN32 
#  	define LOCK
#  	define UNLOCK  
#  	define LOCKEVENTS
#  	define UNLOCKEVENTS
#	define EVENTLOCKTEST		(post_msg_active)
#	define MAXHOSTNAMELEN 		75
#	define IPPORT_USERRESERVED	5000
#	define EWOULDBLOCK		WSAEWOULDBLOCK
#	define ENOBUFS			WSAENOBUFS
#	define ECONNRESET		WSAECONNRESET
#	define ETIMEDOUT		WSAETIMEDOUT
#	define EADDRINUSE		WSAEADDRINUSE
#	define socket_close(S) 		closesocket(S)
#	define socket_ioctl(A,B,C)	ioctlsocket(A,B,C)
#	define MYERRNO			WSAGetLastError()
#	define POST_IO_EV
#	define SYSFREQ			1000000L /* 1 MHz        */
#endif /*WIN32*/


#endif

