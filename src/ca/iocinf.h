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
/*	.11 031692 joh	added declaration for post_msg()		*/
/*	.12 031892 joh	initial rebroadcast delay is now a #define	*/
/*	.13 050492 joh	added exception channel fd set			*/
/*	.14 111792 joh	increased MAXCONNTRIES from 3 to 30		*/
/*	.15 112092 joh	added AST lck count var for VMS			*/
/*	.16 120992 joh	switched to dll list routines			*/
/*	.17 121892 joh	added TCP send buf size var			*/
/*	.18 122192 joh	added outstanding ack var			*/
/*	.19 012094 joh	added minor version (for each server)		*/
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

static char	*iocinfhSccsId = "$Id$";

/*
 * ANSI C includes
 */
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>


/*
 * OS dependent includes
 */
#include "os_depen.h"

/*
 * EPICS includes
 */
#include <cadef.h>
#include <bucketLib.h>
#include <ellLib.h> 
#include <envDefs.h> 

/*
 * CA private includes 
 */
#include "iocmsg.h"

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef NBBY
# define NBBY 8 /* number of bits per byte */
#endif

/*
 * catch when they use really large strings
 */
#define STRING_LIMIT	512

#define INITCHK \
if(!ca_static){ \
        int     s; \
        s = ca_task_initialize(); \
        if(s != ECA_NORMAL){ \
                return s; \
        } \
}

/* throw out requests prior to last ECA_TIMEOUT from ca_pend */
#define	VALID_MSG(PIIU) (piiu->read_seq == piiu->cur_read_seq)

/* perform a build to reconnect this channel ? */
#define VALID_BUILD(CHID)\
(CHID->build_count && CHID->build_type != TYPENOTCONN && CHID->build_value) 

#define SETPENDRECV		{pndrecvcnt++;}
#define CLRPENDRECV(LOCK)	{if(--pndrecvcnt<1){cac_io_done(LOCK); POST_IO_EV;}}


struct udpmsglog{
	long                    nbytes;
	int			valid;
	struct sockaddr_in      addr;
};

/*
 * for use with ca_mux_io()
 */
#define CA_DO_SENDS	1
#define CA_DO_RECVS	2

struct pending_io_event{
  ELLNODE		node;
  void			(*io_done_sub)();
  void			*io_done_arg;
};

union caAddr{
	struct sockaddr_in	inetAddr;
	struct sockaddr		sockAddr;	
};

typedef struct {
	ELLNODE			node;
	union caAddr		srcAddr;
	union caAddr		destAddr;
}caAddrNode;

typedef unsigned long ca_time;

#define MAXCONNTRIES 		60	/* N conn retries on unchanged net */
#define CA_RETRY_PERIOD		5	/* int sec to next keepalive */
#define CA_RECAST_DELAY		1	/* initial int sec to next recast */
#define CA_RECAST_PORT_MASK	0x3	/* random retry interval off port */
#define CA_RECAST_PERIOD 	5	/* ul on retry period long term */
#define CA_CURRENT_TIME 	0

/*
 * for the beacon's recvd hash table
 */
#define BHT_INET_ADDR_MASK		0x7f
typedef struct beaconHashEntry{
	struct beaconHashEntry	*pNext;
	struct in_addr 		inetAddr;
	int			timeStamp;
	int			averagePeriod;
}bhe;

#ifdef vxWorks
typedef struct caclient_put_notify{
	ELLNODE			node;
	PUTNOTIFY		dbPutNotify;
	unsigned long		valueSize; /* size of block pointed to by dbPutNotify */
	void			(*caUserCallback)(struct event_handler_args);
	void			*caUserArg;
	struct ca_static	*pcas;
	int			busy;
}CACLIENTPUTNOTIFY;
#endif /*vxWorks*/

#define MAX_CONTIGUOUS_MSG_COUNT 2

/*
 * ! lock needs to be applied when an id is allocated !
 */
#define CLIENT_ID_WIDTH 	20 /* bits (1 million before rollover) */
#define CLIENT_ID_COUNT		(1<<CLIENT_ID_WIDTH)
#define CLIENT_ID_MASK		(CLIENT_ID_COUNT-1)
#define CLIENT_ID_ALLOC		(CLIENT_ID_MASK&nextBucketId++)

#define SEND_RETRY_COUNT_INIT	100

#define iiuList 	(ca_static->ca_iiuList)
#define piiuCast 	(ca_static->ca_piiuCast)
#define pndrecvcnt	(ca_static->ca_pndrecvcnt)
#define chidlist_pend	(ca_static->ca_chidlist_pend)
#define chidlist_conn	(ca_static->ca_chidlist_conn)
#define chidlist_noreply\
			(ca_static->ca_chidlist_noreply)
#define ioeventlist	(ca_static->ca_ioeventlist)
#define nxtiiu		(ca_static->ca_nxtiiu)
#define free_event_list	(ca_static->ca_free_event_list)
#define pend_read_list	(ca_static->ca_pend_read_list)
#define pend_write_list	(ca_static->ca_pend_write_list)
#define fd_register_func\
			(ca_static->ca_fd_register_func)
#define fd_register_arg	(ca_static->ca_fd_register_arg)
#define post_msg_active	(ca_static->ca_post_msg_active)
#define send_msg_active	(ca_static->ca_send_msg_active)
#define sprintf_buf	(ca_static->ca_sprintf_buf)
#define pBucket		(ca_static->ca_pBucket)
#define nextBucketId	(ca_static->ca_nextBucketId)

#if defined(UNIX) || defined(vxWorks)
#	define readch		(ca_static->ca_readch)
#	define writech		(ca_static->ca_writech)
#endif

#if defined(vxWorks)
#	define io_done_sem	(ca_static->ca_io_done_sem)
#	define evuser		(ca_static->ca_evuser)
#	define client_lock	(ca_static->ca_client_lock)
#	define event_lock	(ca_static->ca_event_lock)
#	define local_chidlist	(ca_static->ca_local_chidlist)
#	define dbfree_ev_list	(ca_static->ca_dbfree_ev_list)
#	define lcl_buff_list	(ca_static->ca_lcl_buff_list)
#	define event_tid	(ca_static->ca_event_tid)
#endif

#if defined(VMS)
#	define io_done_flag	(ca_static->ca_io_done_flag)
#	define peek_ast_buf	(ca_static->ca_peek_ast_buf)
#	define ast_lock_count	(ca_static->ca_ast_lock_count)
#endif

/*
 * one for each task that does a ca import
 */
typedef struct task_var_list{
	ELLNODE			node;
	int			tid;
}TVIU;


/*
 * Ring buffering for both sends and recvs
 */
struct ca_buffer{
  	char			buf[MAX_MSG_SIZE]; /* from iocmsg.h */
	unsigned long		max_msg;
	unsigned long		rdix;
  	unsigned long		wtix;
	int			readLast;
};

#define CAC_RING_BUFFER_INIT(PRBUF, SIZE) \
	assert((SIZE)>sizeof((PRBUF)->buf)); \
	(PRBUF)->max_msg = (SIZE); \
	(PRBUF)->rdix = 0; \
	(PRBUF)->wtix = 0; \
	(PRBUF)->readLast = TRUE; 

#define CAC_RING_BUFFER_READ_ADVANCE(PRBUF, DELTA) \
( 	(PRBUF)->readLast = TRUE, \
	((PRBUF)->rdix += (DELTA)) >= (PRBUF)->max_msg ? \
	(PRBUF)->rdix = 0 :  \
	(PRBUF)->rdix  \
) 

#define CAC_RING_BUFFER_WRITE_ADVANCE(PRBUF, DELTA) \
( 	(PRBUF)->readLast = FALSE, \
	((PRBUF)->wtix += (DELTA)) >= (PRBUF)->max_msg ? \
	(PRBUF)->wtix = 0 :  \
	(PRBUF)->wtix  \
) 


/*
 * One per IOC
 */
typedef struct ioc_in_use{
	ELLNODE			node;
	ELLLIST			chidlist;	/* chans on this connection */
	ELLLIST			destAddr;
	struct ca_static	*pcas;
	int			sock_chan;
	int			sock_proto;
	unsigned		minor_version_number;
	unsigned		contiguous_msg_count;
	unsigned		client_busy;
	char			active;
	struct ca_buffer	send;
	struct ca_buffer	recv;
	struct extmsg		curMsg;
	unsigned		curMsgBytes;
	void			*pCurData;
	unsigned long		curDataMax;
	unsigned long		curDataBytes;
#ifdef __STDC__
	void			(*sendBytes)(struct ioc_in_use *);
	void			(*recvBytes)(struct ioc_in_use *);
	void			(*procInput)(struct ioc_in_use *);
#else /*__STDC__*/
	void			(*sendBytes)();
	void			(*recvBytes)();
	void			(*procInput)();
#endif /*__STDC__*/
	unsigned		read_seq;
	unsigned 		cur_read_seq;
	short			conn_up;	/* boolean: T-conn /F-disconn */
	short			send_needed;	/* CA needs a send */
	char			host_name_str[32];
	unsigned		nconn_tries;
	unsigned		send_retry_count;
	ca_time			next_retry;
	ca_time			retry_delay;

#ifdef VMS	/* for qio ASTs */
	unsigned long		putConvertBufSize;
	void			*pPutConvertBuf;
    	struct sockaddr_in	recvfrom;
    	struct iosb		iosb;
#endif /*VMS*/

#ifdef UNIX
#endif /*UNIX*/

#ifdef vxWorks
#endif /*vxWorks*/

}IIU;


struct  ca_static{
	IIU		*ca_piiuCast;
	ELLLIST		ca_iiuList;
	ELLLIST		ca_ioeventlist;
	ELLLIST		ca_free_event_list;
	ELLLIST		ca_pend_read_list;
	ELLLIST		ca_pend_write_list;
	ELLLIST		activeCASG;
	ELLLIST		freeCASG;
	ELLLIST		activeCASGOP;
	ELLLIST		freeCASGOP;
	long		ca_pndrecvcnt;
	void		(*ca_exception_func)();
	void		*ca_exception_arg;
	void		(*ca_connection_func)();
	void		*ca_connection_arg;
	void		(*ca_fd_register_func)();
	void		*ca_fd_register_arg;
	short		ca_exit_in_progress;  
	unsigned short	ca_post_msg_active; 
	short		ca_repeater_contacted;
	unsigned short	ca_send_msg_active;
	struct in_addr	ca_castaddr;
	char		ca_sprintf_buf[256];
	BUCKET		*ca_pBucket;
	unsigned long	ca_nextBucketId;
	bhe		*ca_beaconHash[BHT_INET_ADDR_MASK+1];
	char		*ca_pUserName;
	char		*ca_pHostName;
#if defined(UNIX) || defined(vxWorks)
	fd_set          ca_readch;  
	fd_set          ca_writech;  
#endif
#if defined(VMS)
	int		ca_io_done_flag;
	char		ca_peek_ast_buf;
	long		ca_ast_lock_count;
#endif
#if defined(vxWorks)
	SEM_ID		ca_io_done_sem;
	SEM_ID		ca_blockSem;
	void		*ca_evuser;
	SEM_ID		ca_client_lock; 
	SEM_ID		ca_event_lock; /* dont allow events to preempt */
	SEM_ID		ca_putNotifyLock;
	int		ca_tid;
	ELLLIST		ca_local_chidlist;
	ELLLIST		ca_dbfree_ev_list;
	ELLLIST		ca_lcl_buff_list;
	ELLLIST		ca_putNotifyQue;
	int		ca_event_tid;
	unsigned	ca_local_ticks;
    	int		recv_tid;
	ELLLIST		ca_taskVarList;
#endif
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
#ifdef __STDC__

void 		cac_send_msg();
void 		ca_mux_io(struct timeval *ptimeout, int flags);
int		repeater_installed();
void 		build_msg(chid chix, int reply_type);
int		ca_request_event(evid monix);
void 		ca_busy_message(struct ioc_in_use *piiu);
void		ca_ready_message(struct ioc_in_use *piiu);
void		noop_msg(struct ioc_in_use *piiu);
void 		issue_claim_channel(struct ioc_in_use *piiu, chid pchan);
void 		issue_identify_client(struct ioc_in_use *piiu);
void 		issue_client_host_name(struct ioc_in_use *piiu);
int		ca_defunct(void);
int 		ca_printf(char *pformat, ...);
void 		manage_conn(int silent);
void 		mark_server_available(struct in_addr *pnet_addr);
void		flow_control(struct ioc_in_use *piiu);
int		broadcast_addr(struct in_addr *pcastaddr);
int		local_addr(int s, struct sockaddr_in *plcladdr);
int		ca_repeater();
void 		cac_recv_task(int tid);
void 		cac_io_done(int lock);
void 		ca_sg_init(void);
void		ca_sg_shutdown(struct ca_static *ca_temp);
int		ca_select_io(struct timeval *ptimeout, int flags);

void caHostFromInetAddr(
	struct in_addr *pnet_addr,
	char		*pBuf,
	unsigned	size
);
int post_msg(
	struct ioc_in_use       *piiu,
	struct in_addr          *pnet_addr,
	char			*pInBuf,
	unsigned long		blockSize
);
int alloc_ioc(
	struct in_addr                  *pnet_addr,
	struct ioc_in_use               **ppiiu
);
unsigned long cacRingBufferWrite(
	struct ca_buffer        *pRing,
	void                    *pBuf,
	unsigned long           nBytes);

unsigned long cacRingBufferRead(
	struct ca_buffer        *pRing,
	void                    *pBuf,
	unsigned long           nBytes);

unsigned long cacRingBufferWriteSize(
	struct ca_buffer 	*pBuf, 
	int 			contiguous);

unsigned long cacRingBufferReadSize(
	struct ca_buffer 	*pBuf, 
	int 			contiguous);

void caSetupAddrList(
	ELLLIST *pList, 
	int socket);

void caDiscoverInterfaces(
	ELLLIST *pList, 
	int socket,
	int port);

void caAddConfiguredAddr(
	ELLLIST *pList, 
	ENV_PARAM *pEnv,
	int socket,
	int port);

void caPrintAddrList();

char *localUserName();

char *localHostName();

int create_net_chan(
struct ioc_in_use       **ppiiu,
struct in_addr		*pnet_addr,	/* only used by TCP connections */
int                     net_proto
);

int ca_check_for_fp();

#else /*__STDC__*/
int		ca_defunct();
int		repeater_installed();
void 		cac_send_msg();
void 		build_msg();
int		broadcast_addr();
int		local_addr();
void		manage_conn();
void 		noop_msg();
void 		ca_busy_message();
void 		ca_ready_message();
void 		flow_control();
void		host_from_addr();
int		ca_repeater();
void		mark_server_available();
void		issue_claim_channel();
int		ca_request_event();
void		cac_io_done();
int		post_msg();
int		alloc_ioc();
int 		ca_printf();
void 		cac_recv_task();
void 		ca_sg_init();
void		ca_sg_shutdown();
void 		issue_identify_client();
void 		issue_client_host_name();
void 		ca_mux_io();
int		ca_select_io();
unsigned long 	cacRingBufferRead();
unsigned long 	cacRingBufferWrite();
unsigned long 	cacRingBufferWriteSize();
unsigned long 	cacRingBufferReadSize();
char		*localUserName();
char		*localHostName();
void		caSetupAddrList();
void		caDiscoverInterfaces();
void		caAddConfiguredAddr();
void		caPrintAddrList();
int		create_net_chan();
int		ca_check_for_fp();
#endif /*__STDC__*/

/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define S_db_Blocked (M_dbAccess|39) /*Request is Blocked*/

#endif /* this must be the last line in this file */
