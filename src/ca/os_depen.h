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
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/time.h>
#	include <sys/ioctl.h>
#	include <sys/param.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <net/if.h>
#	define CA_OS_CONFIGURED
#endif

#ifdef vxWorks
#	include <vxWorks.h>
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <inetLib.h>
#	include <net/if.h>
#	include <taskLib.h>
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
#       define  __TIME /* dont include VMS CC time.h under MULTINET */
#	include <sys/time.h>
#       include <tcp/errno.h>
#	include <ssdef>
#	include <stsdef>
#	include <iodef.h>
#	include <psldef.h>
#       include <prcdef.h>
#       include <descrip.h>
#ifdef UCX /* GeG 09-DEC-1992 */
#       include         <sys/ucx$inetdef.h>
#       include         <ucx.h>
#else
#	include		<net/if.h>
#       include         <vms/inetiodef.h>
#       include         <sys/ioctl.h>
#endif
#	define CA_OS_CONFIGURED
#endif /*VMS*/

#ifndef CA_OS_CONFIGURED
#       error Please define one of vxWorks, UNIX or VMS
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

/************************************************************************/
/*	Provided to enforce one thread at a time code sections		*/
/*	independent of a particular operating system			*/
/************************************************************************/
#if defined(VMS)
  	/* provides for data structure mutal exclusive lock out	*/
  	/* in the VMS AST environment.				*/
  	/* VMS locking recursion allowed			*/
# 	define	LOCK \
    		{ \
			register long astenblwas; \
   			astenblwas = sys$setast(FALSE); \
			if(astenblwas == SS$_WASSET){ \
				ast_lock_count = 1; \
			} \
			else{ \
				ast_lock_count++; \
			} \
		}
# 	define	UNLOCK \
		ast_lock_count--; \
		if(ast_lock_count <= 0){ \
    			sys$setast(TRUE); \
		}
#  	define	LOCKEVENTS
#  	define	UNLOCKEVENTS
#	define	EVENTLOCKTEST	(post_msg_active!=0)
#endif

#if defined(vxWorks)
#	define	VXTASKIDNONE	0
#  	define	LOCK 		semTake(client_lock, WAIT_FOREVER);
#  	define	UNLOCK  	semGive(client_lock);
#	define	LOCKEVENTS \
	{semTake(event_lock, WAIT_FOREVER); event_tid=(int)taskIdCurrent;}
#  	define	UNLOCKEVENTS \
	{event_tid=VXTASKIDNONE; semGive(event_lock);}
#	define	EVENTLOCKTEST \
(((int)taskIdCurrent)==event_tid || ca_static->recv_tid == (int)taskIdCurrent)
#endif

#if defined(UNIX)
#  	define	LOCK
#  	define	UNLOCK  
#  	define	LOCKEVENTS
#  	define	UNLOCKEVENTS
#	define	EVENTLOCKTEST	(post_msg_active!=0)
#endif

#ifdef vxWorks
#	define VXTHISTASKID 	taskIdSelf()
#	define abort() 	taskSuspend(VXTHISTASKID)
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
#endif

#if defined(UNIX)
#   	define socket_close(S) close(S)
#   	define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif

#if defined(vxWorks)
#   	define socket_close(S) close(S)
#   	define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif

#if defined(VMS)
#	ifdef WINTCP
  		extern int	uerrno;		/* Wallongong errno is uerrno 	*/
# 		define MYERRNO	uerrno
#	else
  		extern volatile int noshare socket_errno;
# 		define MYERRNO	socket_errno
#	endif
#endif

#if defined(vxWorks)
# 	define MYERRNO	(errnoGet()&0xffff)
#endif

#if defined(UNIX)
# 	define MYERRNO	errno
#endif

#ifdef VMS
	struct iosb{
		short 		status;
		unsigned short 	count;
		void 		*device;
	};
	static char	ca_unique_address;
#	define		MYTIMERID  (&ca_unique_address)
#endif


#if defined(vxWorks)
#  	define POST_IO_EV semGive(io_done_sem)
#endif

#if defined(VMS)
#  	define POST_IO_EV sys$setef(io_done_flag)
#endif

#if defined(UNIX)
#  	define POST_IO_EV 
#endif

/* delay for when a poll is used	 				*/
/*	NOTE: DELAYTICKS must be less than TICKSPERSEC	*/
#define DELAYTICKS	50L		/* (adjust units below) */
#define TICKSPERSEC	1000L		/* mili sec per sec	*/

/*
 * order of ops is important here
 * 
 * NOTE: large SYFREQ can cause an overflow 
 */
#	define LOCALTICKS	((SYSFREQ*DELAYTICKS)/TICKSPERSEC)

#if defined(VMS)
# 	define SYSFREQ		10000000L	/* 10 MHz	*/
# 	define TCPDELAY\
	{ \
		static int ef=NULL; \
 		int status; \
		int systim[2]={-LOCALTICKS,~0}; \
  		if(!ef) ef= lib$get_ef(&ef); \
  		status = sys$setimr(ef,systim,NULL,MYTIMERID,NULL); \
  		if(~status&STS$M_SUCCESS)lib$signal(status); \
  		status = sys$waitfr(ef); \
  		if(~status&STS$M_SUCCESS)lib$signal(status); \
	};
#endif

#if defined(vxWorks)
# 	define SYSFREQ		((long) sysClkRateGet())  /* usually 60 Hz */
# 	define TCPDELAY 	taskDelay(ca_static->ca_local_ticks);	
# 	define time(A) 		(tickGet()/SYSFREQ)
#endif

#if defined(UNIX)
#	define SYSFREQ		1000000L	/* 1 MHz	*/
	/*
	 * this version of TCPDELAY copies tcpdelayval into temporary storage
	 * just in case the system modifies the timeval arg to select
 	 * in the future as is hinted in the select man page.
	 */
#	define TCPDELAY \
	{ \
		struct timeval dv; \
		dv = tcpdelayval; \
		if(select(0,NULL,NULL,NULL,&dv)<0){ \
			if(MYERRNO != EINTR){ \
				ca_printf("TCPDELAY errno was %d\n", errno); \
			} \
		} \
	} 
#	ifdef CA_GLBLSOURCE
		struct timeval notimeout = {0,0};
		struct timeval tcpdelayval = {0,LOCALTICKS};
#	else
		extern struct timeval notimeout;
		extern struct timeval tcpdelayval;
#	endif
#endif



#endif
