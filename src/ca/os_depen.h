/*
 *	O S _ D E P E N . H
 *
 *
 *	OS dependent stuff for channel access
 *	Author Jeffrey O. Hill
 *
 *	History
 *	.01 joh	110190	Moved to this file
 *
 */

#ifndef INCos_depenh
#define INCos_depenh

#ifdef vxWorks
#	ifndef INCfast_lockh
#		include <fast_lock.h>
#	endif
#endif

/************************************************************************/
/*	Provided to enforce one thread at a time code sections		*/
/*	independent of a particular operating system			*/
/************************************************************************/
#ifdef VMS
  /* provides for data structure mutal exclusive lock out	*/
  /* in the VMS AST environment.				*/
  /* note: the following must allways be used together 		*/
# define	LOCK\
    {register long astenblwas;\
    astenblwas = sys$setast(FALSE);
# define	UNLOCK\
    if(astenblwas == SS$_WASSET)sys$setast(TRUE);}
#endif

#ifdef vxWorks
#  define	LOCK 	FASTLOCK(&client_lock);
#  define	UNLOCK  FASTUNLOCK(&client_lock);
#endif

#ifdef UNIX
#  define	LOCK
#  define	UNLOCK  
#endif

#ifdef vxWorks
#define VXTHISTASKID 	taskIdCurrent
extern int 		taskIdCurrent;
#define abort() taskSuspend(VXTHISTASKID);
#endif

#ifdef vxWorks
#  define memcpy(D,S,N)	bcopy(S,D,N)
#  define memset(D,V,N) bfill(D,N,V)
#  define printf	logMsg
#endif

#ifdef VMS
#ifdef WINTCP	/* Wallangong */
/* (the VAXC runtime lib has its own close */
# define socket_close(S) netclose(S)
# define socket_ioctl(A,B,C) ioctl(A,B,C) 
#else
#endif
#endif

#ifdef UNIX
#   define socket_close(S) close(S)
#   define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif

#ifdef vxWorks
#   define socket_close(S) close(S)
#   define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif

#ifdef VMS
#ifdef WINTCP
  extern int		uerrno;		/* Wallongong errno is uerrno 	*/
# define MYERRNO	uerrno
#else
  extern volatile int noshare socket_errno;
# define MYERRNO	socket_errno
#endif
#endif

#ifdef vxWorks
# define MYERRNO	(errnoGet()&0xffff)
#endif

#ifdef UNIX
  extern int		errno;
# define MYERRNO	errno
#endif

#ifdef VMS
struct iosb{
	short 		status;
	unsigned short 	count;
	void 		*device;
};
static char	ca_unique_address;
#define		MYTIMERID  (&ca_unique_address)
#endif

struct 	timeval{
  	unsigned long	tv_sec;
  	unsigned long	tv_usec;
};

#ifdef vxWorks
#  define POST_IO_EV vrtxPost(&io_done_flag, TRUE)
#endif
#ifdef VMS
#  define POST_IO_EV sys$setef(io_done_flag)
#endif
#ifdef UNIX
#  define POST_IO_EV 
#endif

/* 50 mS delay for TCP to finish transmitting 				*/
/* select wakes you if message is only partly here 			*/
/* so this wait free's up the processor until it completely arrives 	*/
/*	NOTE: DELAYVAL must be less than 1.0		*/
#define DELAYVAL	0.250		/* 250 mS	*/

#ifdef	VMS
# define SYSFREQ	10000000	/* 10 MHz	*/
# define TCPDELAY\
{float delay = DELAYVAL; static int ef=NULL;\
 int status; int systim[2]={-SYSFREQ*DELAYVAL,~0};\
  if(!ef)ef= lib$get_ef(&ef);\
  status = sys$setimr(ef,systim,NULL,MYTIMERID,NULL);\
  if(~status&STS$M_SUCCESS)lib$signal(status);\
  status = sys$waitfr(ef);\
  if(~status&STS$M_SUCCESS)lib$signal(status);\
};
#endif

#ifdef vxWorks
# 	define SYSFREQ		sysClkRateGet()	/* usually 60 Hz */
# 	define TCPDELAY 	taskDelay((unsigned int)DELAYVAL*SYSFREQ);	
# 	define time(A) 		(tickGet()/SYSFREQ)
#endif

#ifdef UNIX
#	define SYSFREQ	1000000		/* 1 MHz	*/
#	define TCPDELAY {if(select(0,NULL,NULL,NULL,&tcpdelayval)<0)abort();}
#  	ifndef CA_GLBLSOURCE
		extern struct timeval tcpdelayval;
#	else
		struct timeval tcpdelayval = {0,(unsigned int)DELAYVAL*SYSFREQ};
#	endif
#endif


#endif
