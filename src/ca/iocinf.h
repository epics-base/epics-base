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
#ifndef INCiocinfh  
#define INCiocinfh


#ifndef INClstLibh  
#include <lstLib.h> 
#endif

#ifndef _TYPES_
#  include	<types.h>
#endif

#ifndef __IN_HEADER__
#include	<in.h>
#endif
              
#ifdef VMS
# include	<ssdef>
#endif

#ifndef INCos_depenh
#	include	<os_depen.h>
#endif

void send_msg();

/* throw out requests prior to last ECA_TIMEOUT from ca_pend */
#define	VALID_MSG(PIIU) (piiu->read_seq == piiu->cur_read_seq)

/* perform a build to reconnect this channel ? */
#define VALID_BUILD(CHID)\
(CHID->build_count && CHID->build_type != TYPENOTCONN && CHID->build_value) 


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

enum channel_state{cs_never_conn, cs_prev_conn, cs_conn, closed};

#define SETPENDRECV	{pndrecvcnt++;}
#define CLRPENDRECV	{if(--pndrecvcnt<1){IODONESUB; POST_IO_EV;}}

/* size of object in bytes rounded up to nearest long word */
#define	QUAD_ROUND(A)	(((A)+3)>>2)
#define	QUAD_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define	BI_ROUND(A)	(((A)+1)>>1)
#define	BI_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))
/*
#define	MAX(A,B)	(((A)>(B))?(A):(B))
#define MIN(A,B)	(((A)>(B))?(B):(A))
*/
/************************************************************************/
/*	Structures							*/
/************************************************************************/

/* stk must be above and contiguous with buf ! */
struct buffer{
  unsigned long		stk;
  char			buf[MAX_MSG_SIZE];	/* from iocmsg.h */
};


/* 	indexs into the ioc in use table	*/ 
#define	MAXIIU		25
#define INVALID_IIU	(MAXIIU+1)
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
#define ioeventlist	(ca_static->ca_ioeventlist)
#define nxtiiu		(ca_static->ca_nxtiiu)
#define free_event_list	(ca_static->ca_free_event_list)
#define pend_read_list	(ca_static->ca_pend_read_list)
#define fd_register_func\
			(ca_static->ca_fd_register_func)
#define fd_register_arg	(ca_static->ca_fd_register_arg)
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
#define local_chidlist	(ca_static->ca_local_chidlist)
#define dbfree_ev_list	(ca_static->ca_dbfree_ev_list)
#endif
#ifdef VMS
#define io_done_flag	(ca_static->ca_io_done_flag)
#endif

struct  ca_static{
  unsigned short	ca_nxtiiu;
  long			ca_pndrecvcnt;
  LIST			ca_ioeventlist;
  void			(*ca_exception_func)();
  void			*ca_exception_arg;
  void			(*ca_connection_func)();
  void			*ca_connection_arg;
  void			(*ca_fd_register_func)();
  void			(*ca_fd_register_arg)();
  short			ca_exit_in_progress;  
  unsigned short	ca_post_msg_active; 
  LIST			ca_free_event_list;
  LIST			ca_pend_read_list;
  short			ca_repeater_contacted;
#ifdef UNIX
  fd_set                ca_readch;  
#endif
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
  LIST			ca_local_chidlist;
  LIST			ca_dbfree_ev_list;
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
    LIST		chidlist;		/* chans on this connection */
    unsigned		nconn_wait;		/* number delays before try */
    unsigned		nconn_tries;		/* number of times conn was tried */
    int			conn_up;		/* boolean: T-conn /F-disconn */
    unsigned		count_to_refresh;	/* ndelays to conn retry */
#define MAXCONNTRIES 3
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
#ifdef CA_GLBLSOURCE
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



#endif
