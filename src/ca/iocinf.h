/* $Id */
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
/************************************************************************/

/*
 * $Log$
 * Revision 1.81  1999/11/18 16:15:42  mrk
 * changes for iocCore port
 *
 * Revision 1.80  1999/11/08 17:14:43  jhill
 * added prenthesis around arguments to VALID_MSG macro
 *
 * Revision 1.79  1999/11/08 17:01:43  jhill
 * fixed problem with VALID_MSG(PIIU) macro
 *
 * Revision 1.78  1999/10/14 23:22:13  jhill
 * dont detect a flow control situation when select is telling us there is
 * something to read when there isnt anything there
 *
 * Revision 1.77  1999/09/14 23:38:18  jhill
 * added ca_vprintf() function
 *
 * Revision 1.76  1999/09/02 21:44:49  jhill
 * improved the way that socket error numbers are converted to strings,
 * changed () to (void) in func proto, and fixed missing parameter to
 * checkConnWatchdogs() bug resulting from this
 *
 * Revision 1.75  1999/07/16 17:02:06  jhill
 * added congestion thresh parm for search alg
 *
 * Revision 1.74.4.1  1999/07/15 20:52:38  jhill
 * added congestion thresh to search sched alg
 *
 * Revision 1.74  1998/10/07 14:32:52  jba
 * Modified log message.
 *
 * Revision 1.73  1998/09/24 21:22:53  jhill
 *  CLR_CA_TIME() now correctly zeros the delay
 *
 * Revision 1.72  1998/06/16 01:07:56  jhill
 * removed caHostFromInetAddr
 *
 * Revision 1.71  1998/05/05 16:04:19  jhill
 * added lock count var
 *
 * Revision 1.70  1998/04/13 19:14:34  jhill
 * fixed task variable problem
 *
 * Revision 1.69  1998/03/12 20:39:10  jhill
 * fixed problem where 3.13.beta11 unable to connect to 3.11 with correct native type
 *
 * Revision 1.68  1998/02/20 21:52:21  evans
 * Added an explicit include of tsDefs.h before cadef.h to avoid the
 * functions in it being declared as export and also to avoid its
 * allocating space when it should be declaring a reference.
 *
 * Revision 1.67  1998/02/05 22:30:34  jhill
 * fixed dll export problems
 *
 * Revision 1.66  1997/08/04 23:37:11  jhill
 * added beacon anomaly flag init/allow ip 255.255.255.255
 *
 * Revision 1.64  1997/06/13 09:14:21  jhill
 * connect/search proto changes
 *
 * Revision 1.63  1997/05/05 04:45:25  jhill
 * send_needed => pushPending, and added ca_number_iiu_in_fc
 *
 * Revision 1.62  1997/04/29 06:11:08  jhill
 * use free lists
 *
 * Revision 1.61  1997/04/23 17:05:07  jhill
 * pc port changes
 *
 * Revision 1.60  1997/04/10 19:26:24  jhill
 * asynch connect, faster connect, ...
 *
 * Revision 1.59  1997/01/22 21:10:26  jhill
 * smaller external sym name for VAXC
 *
 * Revision 1.58  1996/11/02 00:50:56  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.57  1996/09/16 16:38:05  jhill
 * local except => except handler
 *
 * Revision 1.56  1996/08/13 23:16:19  jhill
 * removed os specific code
 *
 * Revision 1.55  1996/08/05 19:21:26  jhill
 * removed unused proto
 *
 * Revision 1.54  1996/06/20 21:43:15  jhill
 * restored io_done_sem (removed by cleanup)
 *
 * Revision 1.53  1996/06/20 21:19:35  jhill
 * fixed posix signal problem with "cc -Xc"
 *
 * Revision 1.52  1996/06/19 17:59:07  jhill
 * many 3.13 beta changes
 *
 * Revision 1.51  1995/12/19  19:33:07  jhill
 * function prototype changes
 *
 * Revision 1.50  1995/10/18  16:45:40  jhill
 * Use recast delay greater than one vxWorks tick
 *
 * Revision 1.49  1995/10/12  01:33:12  jhill
 * Initial delay between search frames went from .1 to .01 sec,
 * Added flush pending flag, Make all usage of port be unsigned short.
 *
 * Revision 1.48  1995/09/29  21:55:38  jhill
 * added func proto for cacDisconnectChannel()
 *
 * Revision 1.47  1995/08/22  00:20:27  jhill
 * added KLUDGE def of S_db_Pending
 */

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

#ifdef CA_GLBLSOURCE
#    	define GLBLTYPE
#else
#    	define GLBLTYPE extern
#endif

#if defined(__STDC__) && !defined(VAXC)
#	define VERSIONID(NAME,VERS) \
       		char *EPICS_CAS_VID_ ## NAME = VERS;
#else /*__STDC__*/
#	define VERSIONID(NAME,VERS) \
       		char *EPICS_CAS_VID_/* */NAME = VERS;
#endif /*__STDC__*/

#ifdef CAC_VERSION_GLOBAL
#	define HDRVERSIONID(NAME,VERS) VERSIONID(NAME,VERS)
#else /*CAC_VERSION_GLOBAL*/
#       define HDRVERSIONID(NAME,VERS)
#endif /*CAC_VERSION_GLOBAL*/

HDRVERSIONID(iocinfh, "$Id$")

/*
 * ANSI C includes
 * !!!!! MULTINET/VMS breaks if we include ANSI time.h here !!!!!
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

/*
 * OS dependent includes
 */
#include "osiSock.h"
#include "os_depen.h"

/*
 * EPICS includes
 */
#if defined(epicsExportSharedSymbols)
#error suspect that libCom was not imported
#endif

#include "epicsAssert.h"
#include "bucketLib.h"
#include "ellLib.h"
#include "envDefs.h" 
#include "epicsPrint.h"
#include "tsDefs.h"

#if defined(epicsExportSharedSymbols)
#error suspect that libCom was not imported
#endif

/*
 * this is defined only after we import from libCom above
 */
#define epicsExportSharedSymbols
#include "cadef.h"

/*
 * CA private includes 
 */
#include "addrList.h"
#include "caProto.h"
#include "net_convert.h"

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

/*
 * writable channel_in_use and pending_event pointers
 * for internal use
 */
typedef struct channel_in_use *ciu;
typedef struct pending_event *miu;

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef max 
#define max(A,B) ((A)<(B)?(B):(A))
#endif

#define MSEC_PER_SEC 	1000L
#define USEC_PER_SEC 	1000000L

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
#define	VALID_MSG(PIIU) ((PIIU)->read_seq == (PIIU)->cur_read_seq)

#define SETPENDRECV {pndrecvcnt++;}
#define CLRPENDRECV {if(--pndrecvcnt<1u){POST_IO_EV;}}

struct udpmsglog{
	long                    nbytes;
	int			valid;
	struct sockaddr_in      addr;
};

struct putCvrtBuf{
	ELLNODE			node;
	unsigned long		size;
	void			*pBuf;
};

/*
 * for use with cac_select_io()
 */
#define CA_DO_SENDS	(1<<0)
#define CA_DO_RECVS	(1<<1)	

typedef struct timeval ca_time;

#define LD_CA_TIME(FLOAT_TIME,PCATIME) \
((PCATIME)->tv_sec = (long) (FLOAT_TIME), \
(PCATIME)->tv_usec = (long) ( ((FLOAT_TIME)-(PCATIME)->tv_sec)*USEC_PER_SEC ))

#define CLR_CA_TIME(PCATIME) ((PCATIME)->tv_sec = 0u,(PCATIME)->tv_usec = 0u)

/*
 * these control the duration and period of name resolution
 * broadcasts
 */
#define MAXCONNTRIES 		100	/* N conn retries on unchanged net */

/*
 * A prime number works best here (see comment in retrySearchRequest()
 */
#define INITIALTRIESPERFRAME    1u	/* initial UDP frames per search try */
#define MAXTRIESPERFRAME        64u	/* max UDP frames per search try */

/*
 * NOTE: These must be larger than one vxWorks tick or we will end up
 * using the CPU. A vxWorks tick is usually 1/60th of a sec. The
 * vxWorks implementation of select is not efficient.
 * select() with no delay takes (on a hk nitro 060):
 * 17uS (no fd)
 * 56uS (one UDP read fd)
 * 80uS (two UDP read fd)
 * 113uS (one UDP read/write fd)
 * 185uS (two UDP read/write fd)
 * 3 uS are required to call FD_ZERO (&readMask)
 *	and FD_ZERO (&writeMask)
 * 4 uS required to call cac_gettimeval()
 */
#ifdef iocCore
#define SELECT_POLL_SEARCH	(0.075) /* units sec - polls for search request (4 ticks)*/
#else
#define SELECT_POLL_SEARCH 	(0.025) /* units sec - polls for search request */
#endif
#define SELECT_POLL_NO_SEARCH	(0.5) /* units sec - polls for conn heartbeat */
#define CA_RECAST_DELAY 	SELECT_POLL_SEARCH /* initial delay to next recast (sec) */
#define CA_RECAST_PORT_MASK	0xff	/* random retry interval off port */
#define CA_RECAST_PERIOD 	(5.0)	/* ul on retry period long term (sec) */

/*
 * these two control the period of connection verifies
 * (echo requests) - CA_CONN_VERIFY_PERIOD - and how
 * long we will wait for an echo reply before we
 * give up and flag the connection for disconnect
 * - CA_ECHO_TIMEOUT.
 *
 * CA_CONN_VERIFY_PERIOD is normally obtained from an
 * EPICS environment variable.
 */
#define CA_ECHO_TIMEOUT		5.0	/* (sec) disconn no echo reply tmo */ 
#define CA_CONN_VERIFY_PERIOD	30.0	/* (sec) how often to request echo */

/*
 * only used when communicating with old servers
 */
#define CA_RETRY_PERIOD		5	/* int sec to next keepalive */

#define N_REPEATER_TRIES_PRIOR_TO_MSG	50
#define REPEATER_TRY_PERIOD		(1.0) 

#ifdef iocCore
typedef struct caclient_put_notify{
	ELLNODE			node;
	PUTNOTIFY		dbPutNotify;
	unsigned long		valueSize; /* size of block pointed to by dbPutNotify */
	void			(*caUserCallback)(struct event_handler_args);
	void			*caUserArg;
	struct CA_STATIC	*pcas;
	int			busy;
}CACLIENTPUTNOTIFY;
#endif /*iocCore*/

/*
 * this determines the number of messages received
 * without a delay in between before we go into 
 * monitor flow control
 *
 * turning this down effect maximum throughput
 * because we dont get an optimal number of bytes 
 * per network frame 
 */
#define MAX_CONTIGUOUS_MSG_COUNT 10 

/*
 * ! lock needs to be applied when an id is allocated !
 */
#define CLIENT_HASH_TBL_SIZE	(1<<12)
#define CLIENT_ID_WIDTH 	28 /* bits */
#define CLIENT_ID_COUNT		(1<<CLIENT_ID_WIDTH)
#define CLIENT_ID_MASK		(CLIENT_ID_COUNT-1)
#define CLIENT_FAST_ID_ALLOC	(CLIENT_ID_MASK&nextFastBucketId++)
#define CLIENT_SLOW_ID_ALLOC	(CLIENT_ID_MASK&nextSlowBucketId++)

#define SEND_RETRY_COUNT_INIT	100

#define iiuList 	(ca_static->ca_iiuList)
#define piiuCast 	(ca_static->ca_piiuCast)
#define pndrecvcnt	(ca_static->ca_pndrecvcnt)
#define ioeventlist	(ca_static->ca_ioeventlist)
#define nxtiiu		(ca_static->ca_nxtiiu)
#define pend_read_list	(ca_static->ca_pend_read_list)
#define pend_write_list	(ca_static->ca_pend_write_list)
#define fd_register_func\
			(ca_static->ca_fd_register_func)
#define fd_register_arg	(ca_static->ca_fd_register_arg)
#define post_msg_active	(ca_static->ca_post_msg_active)
#define sprintf_buf	(ca_static->ca_sprintf_buf)
#define pSlowBucket	(ca_static->ca_pSlowBucket)
#define pFastBucket	(ca_static->ca_pFastBucket)
#define nextSlowBucketId (ca_static->ca_nextSlowBucketId)
#define nextFastBucketId (ca_static->ca_nextFastBucketId)

#if defined(iocCore)
#define POLLDELAY .05
#	define io_done_sem      (ca_static->ca_io_done_sem)
#	define evuser		(ca_static->ca_evuser)
#	define client_lock	(ca_static->ca_client_lock)
#	define local_chidlist	(ca_static->ca_local_chidlist)
#	define lcl_buff_list	(ca_static->ca_lcl_buff_list)
#	define lock_tid	(ca_static->ca_lock_tid)
#	define lock_count (ca_static->ca_lock_count)
#endif

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

#define TAG_CONN_DOWN(PIIU) \
( \
/* ca_printf("Tagging connection down at %d in %s\n", __LINE__, __FILE__), */ \
(PIIU)->state = iiu_disconnected\
)

enum iiu_conn_state{iiu_connecting, iiu_connected, iiu_disconnected};

/*
 * One per IOC
 */
typedef struct ioc_in_use{
	ELLNODE			node;
	ELLLIST			chidlist;	/* chans on this connection */
	ELLLIST			destAddr;
	ca_time			timeAtLastRecv;
	ca_time			timeAtSendBlock;
	ca_time			timeAtEchoRequest;
	unsigned long		curDataMax;
	unsigned long		curDataBytes;
	struct ca_buffer	send;
	struct ca_buffer	recv;
	caHdr			curMsg;
	struct CA_STATIC	*pcas;
	void			*pCurData;
	unsigned long   (*sendBytes)(struct ioc_in_use *);
	unsigned long	(*recvBytes)(struct ioc_in_use *);
	void		(*procInput)(struct ioc_in_use *);
	SOCKET			sock_chan;
	int			sock_proto;
	unsigned		minor_version_number;
	unsigned		contiguous_msg_count;
	unsigned		curMsgBytes;
	unsigned		read_seq;
	unsigned 		cur_read_seq;
	unsigned		minfreespace;
	char			host_name_str[32];
	unsigned char		state;   /* for use with iiu_conn_state enum */

	/*
	 * bit fields placed together for better packing density
	 */
	unsigned		client_busy:1;
	unsigned		echoPending:1; 
	unsigned		sendPending:1;
	unsigned 		claimsPending:1; 
	unsigned		recvPending:1;
	unsigned		pushPending:1;
	unsigned		beaconAnomaly:1;
}IIU;

/*
 * for the beacon's recvd hash table
 */
#define BHT_INET_ADDR_MASK		0xff
typedef struct beaconHashEntry{
	struct beaconHashEntry	*pNext;
	IIU			*piiu;
	struct sockaddr_in	inetAddr;
	ca_time			timeStamp;
	ca_real			averagePeriod;
}bhe;

/*
 * This struct allocated off of a free list 
 * so that the select() ctx is thread safe
 */
typedef struct {
	ELLNODE		node;
	fd_set          readMask;  
	fd_set          writeMask;  
}caFDInfo;

typedef struct  CA_STATIC {
	ELLLIST		ca_iiuList;
	ELLLIST		ca_ioeventlist;
	ELLLIST		ca_pend_read_list;
	ELLLIST		ca_pend_write_list;
	ELLLIST		activeCASG;
	ELLLIST		activeCASGOP;
	ELLLIST		putCvrtBuf;
	ELLLIST		fdInfoFreeList;
	ELLLIST		fdInfoList;
	ca_time		currentTime;
	ca_time		programBeginTime;
	ca_time		ca_conn_next_retry;
	ca_time		ca_conn_retry_delay;
	ca_time		ca_last_repeater_try;
	ca_real		ca_connectTMO;
	IIU		*ca_piiuCast;
	void		(*ca_exception_func)
				(struct exception_handler_args);
	const void	*ca_exception_arg;
	int		(*ca_printf_func)(const char *pformat, va_list args);
	void		(*ca_fd_register_func)
				(void *, int, int);
	const void	*ca_fd_register_arg;
	char		*ca_pUserName;
	char		*ca_pHostName;
	BUCKET		*ca_pSlowBucket;
	BUCKET		*ca_pFastBucket;
	bhe		*ca_beaconHash[BHT_INET_ADDR_MASK+1];
	void		*ca_ioBlockFreeListPVT;
	void		*ca_sgFreeListPVT;
	void		*ca_sgopFreeListPVT;
	ciu		ca_pEndOfBCastList;
	unsigned 	ca_search_responses; /* num valid search resp within seq # */
	unsigned 	ca_search_tries; /* num search tries within seq # */
    unsigned    ca_search_tries_congest_thresh; /* one half N tries w congest */
	unsigned	ca_search_retry; /* search retry seq number */
	unsigned	ca_min_retry; /* min retry no so far */
	unsigned	ca_frames_per_try; /* # of UDP frames per search try */
	unsigned 	ca_pndrecvcnt;
	unsigned 	ca_nextSlowBucketId;
	unsigned 	ca_nextFastBucketId;
	unsigned	ca_repeater_tries;
	unsigned	ca_number_iiu_in_fc;
	unsigned short	ca_server_port;
	unsigned short	ca_repeater_port;
	unsigned short 	ca_search_retry_seq_no; /* search retry seq number */
	unsigned short 	ca_seq_no_at_list_begin; /* search retry seq number at beg of list*/
	char		ca_sprintf_buf[256];
	char		ca_new_err_code_msg_buf[128u];
	unsigned 	ca_post_msg_active:1; 
	unsigned 	ca_manage_conn_active:1; 
	unsigned 	ca_repeater_contacted:1;
	unsigned 	ca_flush_pending:1;
#if defined(iocCore)
	semId		ca_io_done_sem;
	semId		ca_blockSem;
	semId		ca_client_lock; 
	semId		ca_putNotifyLock;
	ELLLIST		ca_local_chidlist;
	ELLLIST		ca_lcl_buff_list;
	ELLLIST		ca_putNotifyQue;
	ELLLIST		ca_taskVarList;
	void		*ca_evuser;
	void		*ca_dbMonixFreeList;
	threadId	ca_lock_tid;
	unsigned	ca_lock_count;
	threadId	ca_tid;
	threadId	recv_tid;
#endif
}CA_STATIC;



#define CASG_MAGIC      0xFAB4CAFE

/*
 * one per outstanding op
 */
typedef struct{
        ELLNODE		node;
        CA_SYNC_GID	id;
        void		*pValue;
        unsigned long	magic;
        unsigned long	seqNo;
}CASGOP;


/*
 * one per synch group
 */
typedef struct{
        ELLNODE                 node;
        CA_SYNC_GID		id;
        unsigned long           magic;
        unsigned long           opPendCount;
        unsigned long           seqNo;
        /*
         * Asynch Notification
         */
#	ifdef iocCore
        semId          sem;
#	endif /*iocCore*/
}CASG;


/*
 *	GLOBAL VARIABLES
 *	There should only be one - add others to ca_static above -joh
 */

#include "caOsDependent.h"
CA_OSD_CA_STATIC

/*
 * CA internal functions
 *
 */

void cac_mux_io(
	CA_STATIC *ca_static,
	struct timeval *ptimeout,
	unsigned iocCloseAllowed);

int	repeater_installed(CA_STATIC *ca_static);
int	search_msg( ciu chix, int reply_type);
int	ca_request_event( evid monix);
int	ca_busy_message(struct ioc_in_use *piiu);
int	ca_ready_message(struct ioc_in_use *piiu);
void noop_msg(struct ioc_in_use *piiu);
int echo_request( struct ioc_in_use *piiu, ca_time *pCurrentTime);
int	issue_claim_channel(chid pchan);
void issue_identify_client(struct ioc_in_use *piiu);
void issue_client_host_name(struct ioc_in_use *piiu);
int	ca_defunct(void);
epicsShareFunc int epicsShareAPI ca_printf (const char *pformat, ...);
epicsShareFunc int epicsShareAPI ca_vPrintf (const char *pformat, va_list args);
void manage_conn(CA_STATIC *ca_static);
void checkConnWatchdogs(CA_STATIC *ca_static, unsigned closeAllowed);
void mark_server_available(
	CA_STATIC *ca_static,
	const struct sockaddr_in *pnet_addr);

void flow_control_on(struct ioc_in_use *piiu);
void flow_control_off(struct ioc_in_use *piiu);
epicsShareFunc void epicsShareAPI ca_repeater(void);
#ifdef iocCore
typedef struct cac_recv_taskArgs {
    CA_STATIC *pcas;
    threadId  tcas;
}cac_recv_taskArgs;
void    cac_recv_task(cac_recv_taskArgs *args);
#endif
void ca_sg_init(CA_STATIC *ca_static);
void ca_sg_shutdown(CA_STATIC *ca_static);
int  cac_select_io(CA_STATIC *ca_static, struct timeval *ptimeout, int flags);
int post_msg(
	struct ioc_in_use       *piiu,
	const struct sockaddr_in	*pnet_addr,
	char			*pInBuf,
	unsigned long		blockSize
);
int alloc_ioc(
	CA_STATIC *ca_static,
	const struct sockaddr_in	*pina,
	struct ioc_in_use		**ppiiu
);
unsigned long cacRingBufferWrite(
	struct ca_buffer        *pRing,
	const void		*pBuf,
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

void caIOBlockListFree(
	CA_STATIC *ca_static,
	ELLLIST *pList,
	chid    chan,
	int     cbRequired,
	int     status);

char *localUserName(void);

char *localHostName(void);

int create_net_chan(
CA_STATIC *ca_static,
struct ioc_in_use       	**ppiiu,
const struct sockaddr_in	*pina, /* only used by TCP connections */
int                     	net_proto
);

void caSetupBCastAddrList ( ELLLIST *pList, SOCKET sock, unsigned short port);

int ca_os_independent_init (CA_STATIC *ca_static);

void freeBeaconHash(struct CA_STATIC *ca_temp);
void removeBeaconInetAddr(
	CA_STATIC *ca_static,
	const struct sockaddr_in *pnet_addr);

bhe *lookupBeaconInetAddr(
	CA_STATIC *ca_static,
	const struct sockaddr_in *pnet_addr);

bhe *createBeaconHashEntry(
	CA_STATIC *ca_static,
	const struct sockaddr_in *pnet_addr,
	unsigned sawBeacon);

void notify_ca_repeater(CA_STATIC *ca_static);

void ca_process_input_queue(CA_STATIC *ca_static);
void cac_block_for_io_completion(CA_STATIC *ca_static,struct timeval *pTV);
void ca_process_exit(CA_STATIC *ca_static);
void ca_spawn_repeater(void);
void cac_gettimeval(struct timeval  *pt);
/* returns A - B in floating secs */
ca_real cac_time_diff(ca_time *pTVA, ca_time *pTVB);
/* returns A + B in integer secs & integer usec */
ca_time cac_time_sum(ca_time *pTVA, ca_time *pTVB);
void caIOBlockFree(CA_STATIC *ca_static,miu pIOBlock);
void clearChannelResources(CA_STATIC *ca_static,unsigned id);
void caSetDefaultPrintfHandler (CA_STATIC *ca_static);
void cacDisconnectChannel( ciu chix);
int caSendMsgPending(CA_STATIC *ca_static);
void genLocalExcepWFL(long stat, const char *ctx, 
	const char *pFile, unsigned line);
#define genLocalExcep(STAT, PCTX) \
genLocalExcepWFL (STAT, PCTX, __FILE__, __LINE__)
void cac_reconnect_channel(CA_STATIC *ca_static, caResId id, short type, unsigned short count);
void retryPendingClaims( IIU *piiu);
void cacSetRetryInterval(CA_STATIC *ca_static, unsigned retryNo);
void addToChanList(ciu chan, IIU *piiu);
void removeFromChanList(ciu chan);
void cac_create_udp_fd(CA_STATIC *ca_static);
double cac_fetch_poll_period(CA_STATIC *ca_static);
void cac_close_ioc ( IIU *piiu);

/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define M_dbAccess      (501 <<16) /*Database Access Routines */
#define S_db_Blocked (M_dbAccess|39) /*Request is Blocked*/
#define S_db_Pending (M_dbAccess|37) /*Request is pending*/

#endif /* this must be the last line in this file */
