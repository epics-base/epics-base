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
/*	.01 08xx87 joh	Init Release					*/
/*	.02 01xx90 joh	fd_set in the UNIX version only			*/
/*	.03 060691 joh	Rearanged buffer struct for SPARC port		*/
/*	.04 072391 joh	new lock prevents event preemption on vxWorks	*/
/*	.05 082791 joh	declaration of ca_request_event()		*/
/*	.06 082791 joh	added send message in progress flag		*/
/*	.07 091691 joh	moved channel_state enum to cadef.h for export	*/
/*	.08 102991 joh	added sprintf buffer				*/
/*	.09 111891 joh	added event task id for vxWorks			*/
/*	.10 111991 joh	mobe IODONESUB macro to service.c		*/
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

#define	DONT_COMPILE	@@@@ dont compile in this case @@@@

#if defined(UNIX)
#  	include	<sys/types.h>
#	include	<netinet/in.h>
#elif defined(VMS)
# 	include	<ssdef>
#  	include	<sys/types.h>
#	include	<netinet/in.h>
#elif defined(vxWorks)
#	ifdef V5_vxWorks
#  		include	<vxTypes.h>
#	else
#  		include	<types.h>
#	endif
#	include	<in.h>
#else
	DONT_COMPILE
#endif

#ifndef INClstLibh  
#	include <lstLib.h> 
#endif

#ifndef INCos_depenh
#	include	<os_depen.h>
#endif


/* throw out requests prior to last ECA_TIMEOUT from ca_pend */
#define	VALID_MSG(PIIU) (piiu->read_seq == piiu->cur_read_seq)

/* perform a build to reconnect this channel ? */
#define VALID_BUILD(CHID)\
(CHID->build_count && CHID->build_type != TYPENOTCONN && CHID->build_value) 


#define SETPENDRECV		{pndrecvcnt++;}
#define CLRPENDRECV(LOCK)	{if(--pndrecvcnt<1){cac_io_done(LOCK); POST_IO_EV;}}

/************************************************************************/
/*	Structures							*/
/************************************************************************/
/* stk must be above and contiguous with buf ! */
struct buffer{
  char			buf[MAX_MSG_SIZE];	/* from iocmsg.h */
  unsigned long		stk;
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

typedef unsigned long	ca_time;
#define CA_RETRY_PERIOD	5		/* sec to next keepalive */
#define CA_CURRENT_TIME 0

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
#define post_msg_active	(ca_static->ca_post_msg_active)
#define send_msg_active	(ca_static->ca_send_msg_active)
#define sprintf_buf	(ca_static->ca_sprintf_buf)

#if defined(UNIX)
#	define readch		(ca_static->ca_readch)
#	define writech		(ca_static->ca_writech)
#	define execch		(ca_static->ca_execch)
#elif defined(vxWorks)
#	define io_done_sem	(ca_static->ca_io_done_sem)
#	define evuser		(ca_static->ca_evuser)
#	define client_lock	(ca_static->ca_client_lock)
#	define event_lock	(ca_static->ca_event_lock)
#	define local_chidlist	(ca_static->ca_local_chidlist)
#	define dbfree_ev_list	(ca_static->ca_dbfree_ev_list)
#	define lcl_buff_list	(ca_static->ca_lcl_buff_list)
#	define event_tid	(ca_static->ca_event_tid)
#elif defined(VMS)
#	define io_done_flag	(ca_static->ca_io_done_flag)
#	define peek_ast_buf	(ca_static->ca_peek_ast_buf)
#else
	DONT_COMPILE
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
  void			*ca_fd_register_arg;
  short			ca_exit_in_progress;  
  unsigned short	ca_post_msg_active; 
  LIST			ca_free_event_list;
  LIST			ca_pend_read_list;
  short			ca_repeater_contacted;
  unsigned short	ca_send_msg_active;
  short			ca_cast_available;
  struct in_addr	ca_castaddr;
  char			ca_sprintf_buf[128];
#if defined(UNIX)
  fd_set                ca_readch;  
#elif defined(VMS)
  int			ca_io_done_flag;
  char			ca_peek_ast_buf;
#elif defined(vxWorks)
  SEM_ID		ca_io_done_sem;
  void			*ca_evuser;
  FAST_LOCK		ca_client_lock; 
  FAST_LOCK		ca_event_lock; /* dont allow events to preempt */
  int			ca_tid;
  LIST			ca_local_chidlist;
  LIST			ca_dbfree_ev_list;
  LIST			ca_lcl_buff_list;
  int			ca_event_tid;
#else
  DONT_COMPILE
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
    int			conn_up;		/* boolean: T-conn /F-disconn */
    unsigned		nconn_tries;
    ca_time		next_retry;
    ca_time		retry_delay;
#define MAXCONNTRIES 3
#if defined(VMS)	/* for qio ASTs */
    struct sockaddr_in	recvfrom;
    struct iosb		iosb;
#elif defined(vxWorks)
    int			recv_tid;
#elif defined(UNIX)
#else
    DONT_COMPILE
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

/*
 * CA internal functions
 *
 */
void 		cac_send_msg();
void 		build_msg();
int		broadcast_addr();
int		local_addr();
void		manage_conn();
void 		noop_msg();
void 		ca_busy_message();
void 		ca_ready_message();
void 		flow_control();
char 		*host_from_addr();
int		ca_repeater_task();
void		close_ioc();
void		recv_msg_select();
void		mark_server_available();
void		issue_claim_channel();
void		ca_request_event();
void		cac_io_done();

#endif /* this must be the last line in this file */
