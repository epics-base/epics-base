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

static char *os_depenhSccsId = "$Id$\t$Date$";

#if defined(UNIX)
#	ifndef _sys_time_h
#		include <sys/time.h>
#	endif
#	ifndef _sys_errno_h
#		include <sys/errno.h>
#	endif
#elif defined(vxWorks)
#	ifndef INCvxWorksh
#		include <vxWorks.h>
#	endif
#	ifndef INCfast_lockh
#		include <fast_lock.h>
#	endif
#	ifndef INCtaskLibh
#		include <taskLib.h>
#	endif
#	ifndef V5_vxWorks
		IMPORT ULONG taskIdCurrent;
#	endif
#elif defined(VMS)
#else
	@@@@ dont compile in this case @@@@
#endif


#if     !defined(NULL) || (NULL!=0)
#define NULL            0
#endif  

#if     !defined(FALSE) || (FALSE!=0)
#define FALSE           0
#endif  

#if     !defined(TRUE) || (TRUE!=1)
#define TRUE            1
#endif  

#if     !defined(OK) || (OK!=0)
#define OK            0
#endif  

#if     !defined(ERROR) || (ERROR!=(-1))
#define ERROR            (-1)
#endif  

#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
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
#	define  RECV_ACTIVE(PIIU) (piiu->active)
#elif defined(vxWorks)
#	define	VXTASKIDNONE	0
#  	define	LOCK 		FASTLOCK(&client_lock);
#  	define	UNLOCK  	FASTUNLOCK(&client_lock);
#  	define	LOCKEVENTS 	{FASTLOCK(&event_lock); event_tid=(int)taskIdCurrent;}
#  	define	UNLOCKEVENTS	{event_tid=VXTASKIDNONE; FASTUNLOCK(&event_lock);}
#	define	EVENTLOCKTEST	(FASTLOCKTEST(&event_lock)&&taskIdCurrent==event_tid)
#	define  RECV_ACTIVE(PIIU) (piiu->recv_tid == taskIdCurrent)
#elif defined(UNIX)
#  	define	LOCK
#  	define	UNLOCK  
#  	define	LOCKEVENTS
#  	define	UNLOCKEVENTS
#	define	EVENTLOCKTEST	(post_msg_active!=0)
#	define  RECV_ACTIVE(PIIU) (piiu->active)
#else
	@@@@ dont compile in this case @@@@
#endif

#ifdef vxWorks
#	define VXTHISTASKID 	taskIdSelf()
#	define abort(A) 	taskSuspend(VXTHISTASKID)
#  	define memcpy(D,S,N)	bcopy(S,D,N)
#  	define memset(D,V,N) 	bfill(D,N,V)
#endif


#if defined(VMS)
#	if defined(WINTCP)	/* Wallangong */
	/* (the VAXC runtime lib has its own close */
# 		define socket_close(S) netclose(S)
# 		define socket_ioctl(A,B,C) ioctl(A,B,C) 
#	elif defined(UCX)				/* GeG 09-DEC-1992 */
# 		define socket_close(S) close(S)
# 		define socket_ioctl(A,B,C) ioctl(A,B,C) 
#	else
#	endif
#elif defined(UNIX)
#   	define socket_close(S) close(S)
#   	define socket_ioctl(A,B,C) ioctl(A,B,C) 
#elif defined(vxWorks)
#   	define socket_close(S) close(S)
#   	define socket_ioctl(A,B,C) ioctl(A,B,C) 
#else
	@@@@ dont compile in this case @@@@
#endif

#if defined(VMS)
#	ifdef WINTCP
  		extern int	uerrno;		/* Wallongong errno is uerrno 	*/
# 		define MYERRNO	uerrno
#	else
  		extern volatile int noshare socket_errno;
# 		define MYERRNO	socket_errno
#	endif
#elif defined(vxWorks)
# 	define MYERRNO	(errnoGet()&0xffff)
#elif defined(UNIX)
  	extern int	errno;
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
#ifdef V5_vxWorks
#  	define POST_IO_EV semGive(io_done_sem)
#else
#  	define POST_IO_EV vrtxPost(&io_done_sem->count, TRUE)
#endif
#elif defined(VMS)
#  	define POST_IO_EV sys$setef(io_done_flag)
#elif defined(UNIX)
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
#elif defined(vxWorks)
# 	define SYSFREQ		((long) sysClkRateGet())  /* usually 60 Hz */
# 	define TCPDELAY 	taskDelay(ca_static->ca_local_ticks);	
# 	define time(A) 		(tickGet()/SYSFREQ)
#elif defined(UNIX)
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
		struct timeval tcpdelayval = {0,LOCALTICKS};
#	else
		extern struct timeval tcpdelayval;
#	endif
#endif

#endif
