/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/	
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*									*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date		Programmer	Comments			*/
/*	----		----------	--------			*/
/*	8/87		Jeff Hill	Init Release			*/
/*	1/90		Jeff Hill	fd_set in the UNIX version only	*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	channel access TCPIP interface include file		*/
/*	File:	atcs:[ca]iocinf.h					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc interface include						*/
/*									*/
/*									*/
/*	Special comments						*/	
/*	------- --------						*/
/*	Use GLBLTYPE to define externals so we can change the all at	*/
/*	once from VAX globals to generic externals			*/
/*									*/	
/************************************************************************/
/*_end									*/

#ifndef INClstLibh  
#include <lstLib.h> 
#endif

#ifndef _TYPES_
#  include	<types.h>
#endif

#ifndef __IN_HEADER__
#include	<in.h>
#endif
              
#ifdef vxWorks
#include <fast_lock.h>
#endif


#ifdef VMS
# include	<ssdef>
#endif


#ifdef VMS
/************************************************************************/
/*	Provided to enforce one thread at a time code sections		*/
/*	independent of a particular operating system			*/
/************************************************************************/
  /* note: the following must allways be used together 		*/
  /* provided for data structure mutal exclusive lock out	*/
  /* in the VMS AST environment.				*/
# define	LOCK\
    {register long astenblwas;\
    astenblwas = sys$setast(FALSE);
# define	UNLOCK\
    if(astenblwas == SS$_WASSET)sys$setast(TRUE);}
#else
#  ifdef vxWorks
#    define	LOCK 	FASTLOCK(&client_lock);
#    define	UNLOCK  FASTUNLOCK(&client_lock);
#  else
#    define	LOCK
#    define	UNLOCK  
#  endif
#endif

#define IODONESUB\
{\
  register struct pending_io_event	*pioe;\
  LOCK;\
  if(ioeventlist.count)\
    while(pioe = (struct pending_io_event *) lstGet(&ioeventlist)){\
      (*pioe->io_done_sub)(pioe->io_done_arg);\
      if(free(pioe))abort();\
    }\
  UNLOCK;\
}

#define	VALID_MSG(PIIU) (piiu->read_seq == piiu->cur_read_seq)

#ifdef vxWorks
#  define	SETPENDRECV	{pndrecvcnt++;}
#  define	CLRPENDRECV\
{if(--pndrecvcnt<1){IODONESUB; vrtxPost(&io_done_flag, TRUE);}}
#else
#ifdef VMS
#  define	SETPENDRECV	{pndrecvcnt++;}
#  define	CLRPENDRECV\
{if(--pndrecvcnt<1){IODONESUB; sys$setef(io_done_flag);}}
#else
#  define	SETPENDRECV	{pndrecvcnt++;}
#  define	CLRPENDRECV\
{if(--pndrecvcnt<1){IODONESUB;}}
#endif
#endif

#ifdef WINTCP	/* Wallangong */
/* (the VAXC runtime lib has its own close */
# define socket_close(S) netclose(S)
# define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif
#ifdef UNIX
#   define socket_close(S) close(S)
#   define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif
#ifdef vxWorks
#   define socket_close(S) close(S)
#   define socket_ioctl(A,B,C) ioctl(A,B,C) 
#endif

/* size of object in bytes rounded up to nearest long word */
#define	QUAD_ROUND(A)	(((A)+3)>>2)
#define	QUAD_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define	BI_ROUND(A)	(((A)+1)>>1)
#define	BI_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

#define	MAX(A,B)	(((A)>(B))?(A):(B))
#define MIN(A,B)	(((A)>(B))?(B):(A))

#ifdef vxWorks
#define VXTHISTASKID 	taskIdCurrent
extern int 		taskIdCurrent;
#define abort() taskSuspend(taskIdCurrent);
#endif

#ifdef vxWorks
#  define memcpy(D,S,N)	bcopy(S,D,N)
#  define memset(D,V,N) bfill(D,N,V)
#endif

#ifdef VMS
#ifdef WINTCP
  extern int		uerrno;		/* Wallongong errno is uerrno 	*/
# define MYERRNO	uerrno
#else
  extern volatile int noshare socket_errno;
# define MYERRNO	socket_errno
#endif
#else
# ifdef vxWorks
#   define MYERRNO	(errnoGet()&0xffff)
# else
    extern int		errno;
#   define MYERRNO	errno
# endif
#endif


/************************************************************************/
/*	Structures							*/
/************************************************************************/

/* stk must be above and contiguous with buf ! */
struct buffer{
  unsigned long		stk;
  char			buf[MAX_MSG_SIZE];	/* from iocmsg.h */
};

#ifdef VMS
struct iosb{
short 		status;
unsigned short 	count;
void 		*device;
};
#endif

struct 	timeval{
  unsigned long	tv_sec;
  unsigned long	tv_usec;
};

/* 	Moved in an allocated structure for vxWorks Compatibility	*/ 
#define	MAXIIU		25
#define LOCAL_IIU	(MAXIIU+100)
#define BROADCAST_IIU	0

struct pending_io_event{
  NODE			node;
  void			(*io_done_sub)();
  void			*io_done_arg;
};


#define MAX_CONTIGUOUS_MSG_COUNT 2


#define iiu 		(ca_static->ca_iiu)
#define pndrecvcnt	(ca_static->ca_pndrecvcnt)
#define chidlist_pend	(ca_static->ca_chidlist_pend)
#define chidlist_conn	(ca_static->ca_chidlist_conn)
#define chidlist_noreply\
			(ca_static->ca_chidlist_noreply)
#define eventlist	(ca_static->ca_eventlist)
#define ioeventlist	(ca_static->ca_ioeventlist)
#define nxtiiu		(ca_static->ca_nxtiiu)
#ifdef UNIX
#define readch		(ca_static->ca_readch)
#define writech		(ca_static->ca_writech)
#define execch		(ca_static->ca_execch)
#endif
#define post_msg_active	(ca_static->ca_post_msg_active)
#ifdef vxWorks
#define io_done_flag	(ca_static->ca_io_done_flag)
#define evuser		(ca_static->ca_evuser)
#define client_lock	(ca_static->ca_client_lock)
#define event_buf	(ca_static->ca_event_buf)
#define event_buf_size	(ca_static->ca_event_buf_size)
#endif
#ifdef VMS
#define io_done_flag	(ca_static->ca_io_done_flag)
#endif

struct  ca_static{
  unsigned short	ca_nxtiiu;
  long			ca_pndrecvcnt;
  LIST			ca_chidlist_conn;
  LIST			ca_chidlist_pend;
  LIST			ca_chidlist_noreply;
  LIST			ca_eventlist;
  LIST			ca_ioeventlist;
#ifdef UNIX
  fd_set                ca_readch;  
  fd_set                ca_writech; 
  fd_set                ca_execch;  
#endif
  short			ca_exit_in_progress;  
  unsigned short	ca_post_msg_active;  
#ifdef VMS
  int			ca_io_done_flag;
#endif
#ifdef vxWorks
  int			ca_io_done_flag;
  void			*ca_evuser;
  FAST_LOCK		ca_client_lock;
  void			*ca_event_buf;
  unsigned		ca_event_buf_size;
  int			ca_tid;
#endif
  struct ioc_in_use{
    unsigned		contiguous_msg_count;
    unsigned		client_busy;
    int			sock_proto;
    struct sockaddr_in	sock_addr;
    int			sock_chan;
    int			max_msg;
    struct buffer	*send;
    struct buffer	*recv;
    unsigned		read_seq;
    unsigned 		cur_read_seq;
#ifdef VMS	/* for qio ASTs */
    struct sockaddr_in	recvfrom;
    struct iosb		iosb;
#endif
#ifdef vxWorks
    int			recv_tid;
#endif
  }			ca_iiu[MAXIIU];
};

/*
	GLOBAL VARIABLES
	There should only be one - add others to ca_static above -joh
*/
#ifdef GLBLSOURCE
#  ifdef VAXC
#    define GLBLTYPE globaldef
#  else
#    define GLBLTYPE
#  endif
#else
#  ifdef VAXC
#    define GLBLTYPE globalref
#  else
#    define GLBLTYPE extern
#  endif
#endif

GLBLTYPE
struct ca_static *ca_static;

#ifdef VMS
GLBLTYPE
char		ca_unique_address;
# define	MYTIMERID  (&ca_unique_address)
#endif


/*

Error handlers needed

remote_error(user_arg,chid,type,count)
diconnect(user_arg,chid)
connect(user_arg,chid)
io_done(user_arg)

*/
