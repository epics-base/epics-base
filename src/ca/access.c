/* $Id$	*/
/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*									*/
/*	Author: Jeffrey O. Hill						*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date	Person	Comments					*/
/*	----	------	--------					*/
/*	8/87	joh	Init Release					*/
/*	1/90	joh	switched order in task exit			*/
/*			so events are deleted first then closed		*/
/*	3/90	joh	Changed error message strings			*/
/*			to allocate in this module only			*/
/*			& further improved vx task exit			*/
/*	021291 	joh 	fixed potential problem with strncpy and 	*/
/*			the vxWorks task name.				*/
/*	022291 	joh	read sync not incremented when conn is down	*/
/*	030791	joh	vx lcl event buf free prior to first alloc fix	*/
/*	030791	joh	made lcl buffer safe in ca_event_handler()	*/
/*	031291	joh	added DBR_LONG to ca_put, made UNIX and vxWorks	*/
/*			ca_put less sensitive to future data type 	*/
/*			additions, added better string bounds checking	*/
/*	060591	joh	delinting					*/
/*	061391	joh	RISC alignment in outgoing messages		*/
/*	070191	joh	always use memcpy in ca_put			*/
/*	071291	joh	added CLAIM_CIU message				*/
/*	072391	joh	added event locking for vxWorks			*/
/*	072591	joh	quick POLL in ca_pend_io() should return 	*/
/*			ECA_NORMAL not ECA_TIMEOUT if pend count == 0	*/
/*	082391	joh	check for connection down when reissuing	*/
/*			monitors					*/
/*	090991	joh	converted to v5 vxWorks				*/
/*	092691	joh	check for connection down when inserting msg	*/
/*	111891	joh	better test for ca_pend during an event routine	*/
/*			under vxWorks					*/
/*	111991	joh	dont decr the pend recv count if connected 	*/
/*			before and they are adding a connection handler */
/*			prior to connecting				*/
/* 	111991	joh	selective activation of LOCK with CLRPENDRECV	*/
/*			prevents double LOCK under vxWorks		*/
/*	020392	joh	added calls to taskVarGetPatch to bypass	*/
/*			v5 vxWorks bug.					*/
/*	021392 	joh	dont zero ca_static in the exit handler under	*/
/*			vxWorks since vxWorks may run the exit handler	*/
/*			in the context of another task which has a	*/
/*			valid ca_static of its own.			*/
/*	022692	joh	Use channel state enum to determine if I need	*/
/*			to send channel clear message to the IOC 	*/
/*			instead of the IOC in use conn up flag		*/
/*	031292 	joh	clear pend recv cnt even if its a poll in	*/
/*			ca_pend_io()					*/
/*	031892 	joh	initial broadcast retry delay is now a #define	*/
/*	032092	joh	dont allow them to add an event which does not	*/
/*			select anything (has a nill mask)		*/
/*	041492	joh	Use channel state enum to determine if I need	*/
/*			to send monitor clear message to the IOC 	*/
/*			instead of the IOC in use conn up flag		*/
/*			(did the same thing to the tests in 		*/
/*			issue_get_callback() & request_event())		*/
/*	041492	joh	fixed bug introduced by 022692 when the chan	*/
/*			state enum was used after it was set to		*/
/*			cs_closed					*/
/*	042892	joh	no longer checks the status from free() as	*/
/*			this varies from OS to OS			*/
/*	050492	joh	batch up flow control messages by setting a	*/
/*			send_needed flag				*/
/*	060392	joh	added ca_host_name()				*/
/*	072792	joh	better messages					*/
/*	072792	joh	wrote ca_test_io()				*/
/*	102992	joh	I notice that the vxWorks FP task option cant 	*/
/*			be modified after task creation so CA now wont	*/
/*			init if the task does not have the FP opt set	*/ 
/*	111892	joh	handle the case where no broadcast interface	*/
/*			is available better				*/
/*	111992	joh	added new arg to db_start_events() call		*/
/*	120992	joh	switched to dll list routines			*/
/*	122192	joh	increment outstanding ack count			*/
/*	050593	joh	dont enable deadlock prevention if we are in	*/
/*			post message					*/
/*	070293	joh	set ca_static to nill at the end of		*/
/*			ca_process_exit() under all os and not just	*/
/*			vxWorks						*/
/*	120293	joh	flush in ca_pend_io() if no IO oustanding	*/
/*	121693	joh	fixed bucketLib level memory leak		*/
/*	011394	joh	fixed bucketLib level memory leak (vxWorks)	*/
/*	020494	joh	Added user name protocol			*/
/*      022294  joh     fixed recv task id verify at exit (vxWorks)     */
/*      072895  joh     fixed problem resulting from unsigned long 	*/
/*			tv_sec var in struct timeval in sys/time.h	*/
/*			under HPUX					*/
/************************************************************************/
/*
 * $Log$
 * Revision 1.98  1997/08/04 22:54:28  jhill
 * mutex clean up
 *
 * Revision 1.96  1997/06/13 16:57:38  jhill
 * fixed warning
 *
 * Revision 1.95  1997/06/13 09:14:06  jhill
 * connect/search proto changes
 *
 * Revision 1.94  1997/05/05 04:40:29  jhill
 * send_needed replaced by pushPending flag
 *
 * Revision 1.93  1997/04/29 06:05:57  jhill
 * use free list
 *
 * Revision 1.92  1997/04/23 17:04:57  jhill
 * pc port changes
 *
 * Revision 1.91  1997/04/10 19:26:01  jhill
 * asynch connect, faster connect, ...
 *
 * Revision 1.90  1997/01/22 21:06:30  jhill
 * use genLocalExcepWFL for generateLocalExceptionWithFileAndLine
 *
 * Revision 1.89  1997/01/10 21:02:10  jhill
 * host/user name set is now a NOOP
 *
 * Revision 1.88  1996/11/22 19:05:48  jhill
 * added const to build and connect API
 *
 * Revision 1.87  1996/11/02 00:50:30  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.86  1996/09/16 16:41:47  jhill
 * local except => except handler & ca vers str routine
 *
 * Revision 1.85  1996/09/04 20:02:00  jhill
 * test for non-nill piiu under vxWorks
 *
 * Revision 1.84  1996/07/10 23:30:09  jhill
 * fixed GNU warnings
 *
 * Revision 1.83  1996/07/09 22:43:29  jhill
 * silence gcc warnings and default CLOCKS_PER_SEC if it isnt defined (for sunos4 and gcc)
 *
 * Revision 1.82  1996/06/19 17:58:59  jhill
 * many 3.13 beta changes
 *
 * Revision 1.81  1995/12/19  19:28:11  jhill
 * Dont check the array element count when they add the event (just clip it)
 *
 * Revision 1.80  1995/10/18  16:49:23  jhill
 * recv task is now running at a lower priority than the send task under vxWorks
 *
 * Revision 1.79  1995/10/12  01:30:10  jhill
 * new ca_flush_io() mechanism prevents deadlock when they call
 * ca_flush_io() from within an event routine. Also forces early
 * transmission of leading search UDP frames.
 *
 * Revision 1.78  1995/09/29  21:47:33  jhill
 * alignment fix for SPARC IOC client and changes to prevent running of
 * access rights or connection handlers when the connection is lost just
 * after deleting a channel
 *
 * Revision 1.77  1995/09/01  14:31:32  mrk
 * Fixed bug causing memory problem
 *
 * Revision 1.76  1995/08/23  00:34:06  jhill
 * fixed vxWorks specific SPARC data alignment problem
 *
 * Revision 1.75  1995/08/22  00:15:19  jhill
 * Use 1.0/USEC_PER_SEC and not 1.0e-6
 * Check for S_db_Pending when calling dbPutNotify()
 *
 * Revision 1.74  1995/08/22  00:12:07  jhill
 * *** empty log message ***
 *
 * Revision 1.73  1995/08/14  19:26:10  jhill
 * epicsAPI => epicsShareAPI
 *
 * Revision 1.72  1995/08/12  00:23:32  jhill
 * check for res id in use, epicsEntry, dont wait for itsy bitsy delay
 * in ca_pend_event(), better clean up when monitor is deleted and
 * we are disconnected
 *
 */
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC high level access routines				*/
/*	File:	access.c						*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc high level access routines					*/
/*									*/
/*									*/
/*	Special comments						*/
/*	------- --------						*/
/*	Things that could be done to improve this code			*/
/*	1)	Allow them to recv channel A when channel B changes	*/
/*									*/
/************************************************************************/
/*_end									*/

static char *sccsId = "@(#) $Id$";

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

/*
 * allocate db_access message strings here
 */
#define DB_TEXT_GLBLSOURCE

/*
 * allocate header version strings here
 */
#define CAC_VERSION_GLOBAL

#include 	"iocinf.h"
#include	"sigPipeIgnore.h"

#include 	"freeList.h"

#ifdef vxWorks
#include 	"dbEvent.h"
#endif


/****************************************************************/
/*	Macros for syncing message insertion into send buffer	*/
/****************************************************************/
#define EXTMSGPTR(PIIU)\
 	((caHdr *) &(PIIU)->send.buf[(PIIU)->send.wtix])




/****************************************************************/
/*	Macros for error checking				*/
/****************************************************************/

/*
 *	valid type check filters out disconnected channel
 */

#define CHIXCHK(CHIX) \
{ \
	if( (CHIX)->state != cs_conn || INVALID_DB_REQ((CHIX)->type) ){ \
		return ECA_BADCHID; \
	} \
}

/* type to detect them using deallocated channel in use block */
#define TYPENOTINUSE (-2)

/* allow them to add monitors to a disconnected channel */
#define LOOSECHIXCHK(CHIX) \
{ \
	if( (CHIX)->type==TYPENOTINUSE ){ \
		return ECA_BADCHID; \
	} \
}

static caHdr	nullmsg;

/*
 * local functions
 */

LOCAL int cac_alloc_msg_no_flush(
struct ioc_in_use 	*piiu,
unsigned		extsize,
caHdr		**ppMsg
);
LOCAL int	issue_get_callback(
evid 		monix, 
unsigned 	cmmd
);
#ifdef vxWorks
LOCAL void ca_event_handler(
void		*usrArg,
struct dbAddr	*paddr,
int		hold,
db_field_log	*pfl
);
LOCAL void ca_put_notify_action(PUTNOTIFY *ppn);
#endif
LOCAL void 	ca_pend_io_cleanup();

static int issue_ca_array_put
(
unsigned	cmd,
unsigned	id,
chtype		type,
unsigned long	count,
chid		chix,
const void	*pvalue
);
LOCAL void ca_default_exception_handler(struct exception_handler_args args);

LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
caHdr			*pmsg,
const void		*pext
);

LOCAL int cac_push_msg_no_block(
struct ioc_in_use 	*piiu,
caHdr			*pmsg,
const void		*pext
);

LOCAL void cac_add_msg (IIU *piiu);

#ifdef CONVERSION_REQUIRED 
LOCAL void *malloc_put_convert(unsigned long size);
LOCAL void free_put_convert(void *pBuf);
#endif

LOCAL miu caIOBlockCreate(void);

LOCAL int check_a_dbr_string(const char *pStr, const unsigned count);


/*
 *
 * 	cac_push_msg()
 *
 */ 
LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
caHdr			*pmsg,
const void		*pext
)
{
	int		contig = piiu->sock_proto != IPPROTO_TCP;
	caHdr		msg;
	unsigned 	bytesAvailable;
	unsigned 	actualextsize;
	unsigned 	extsize;
	unsigned	msgsize;
	unsigned 	bytesSent;

	msg = *pmsg;
	actualextsize = pmsg->m_postsize;
	extsize = CA_MESSAGE_ALIGN(pmsg->m_postsize);
	msg.m_postsize = htons((ca_uint16_t)extsize);
	msgsize = extsize+sizeof(msg);


	LOCK;

	/*
	 * 	Force contiguous header and body 
	 *
	 * o	Forces hdr and bdy into the same frame if UDP
	 *
	 * o	Does not allow interleaved messages
	 *	caused by removing recv backlog during 
	 *	send under UNIX.
	 *
	 * o	Does not allow for messages larger than the
	 *	ring buffer size.
	 */
	if (msgsize>piiu->send.max_msg) {
		return ECA_TOLARGE;
	}

	bytesAvailable = cacRingBufferWriteSize(&piiu->send, contig);

	if (bytesAvailable<msgsize) {
		/*
		 * try to send first so that we avoid the
		 * overhead of select() in high throughput
		 * situations
		 */
		if (piiu->state==iiu_connected) {
			(*piiu->sendBytes)(piiu);
		}
		bytesAvailable = 
			cacRingBufferWriteSize(&piiu->send, contig);

		while(TRUE){
			struct timeval	itimeout;

			/*
			 * record the time if we end up blocking so that
			 * we can time out
			 */
			if (bytesAvailable>=msgsize){
				piiu->sendPending = FALSE;
				break;
			}
			else {
				if (!piiu->sendPending) {
					piiu->timeAtSendBlock = 
						ca_static->currentTime;
					piiu->sendPending = TRUE;
				}
			}

			UNLOCK;

			/*
			 * if connection drops request
			 * cant be completed
			 */
			if (piiu->state!=iiu_connected) {
				return ECA_BADCHID;
			}

			LD_CA_TIME (cac_fetch_poll_period(), &itimeout);
			cac_mux_io (&itimeout);

			LOCK;

			bytesAvailable = cacRingBufferWriteSize(
						&piiu->send, contig);
		}
	}


	/*
	 * push the header onto the ring 
	 */
	bytesSent = cacRingBufferWrite(
				&piiu->send, 
				&msg, 
				sizeof(msg));
	assert(bytesSent == sizeof(msg));

	/*
	 * push message body onto the ring
	 *
	 * (optionally encode in netrwork format as we send)
	 */
	if (extsize>0u) {
		bytesSent = cacRingBufferWrite(
					&piiu->send, 
					pext, 
					actualextsize);
		assert(bytesSent == actualextsize);
		/*
		 * force pad bytes at the end of the message to nill
		 * if present (this avoids messages from purify)
		 */
		{
			static 		nullBuff[32];
			unsigned	n;

			n = extsize-actualextsize;
			if (n) {
				assert(n<=sizeof(nullBuff));
				bytesSent = cacRingBufferWrite(
						&piiu->send, 
						nullBuff, 
						n);
				assert(bytesSent == n);
			}
		}
	}

	UNLOCK;

	return ECA_NORMAL;
}


/*
 *
 * 	cac_push_msg_no_block()
 *
 */ 
LOCAL int cac_push_msg_no_block(
struct ioc_in_use 	*piiu,
caHdr			*pmsg,
const void		*pext)
{
	int		contig = piiu->sock_proto != IPPROTO_TCP;
	caHdr		msg;
	unsigned 	bytesAvailable;
	unsigned 	actualextsize;
	unsigned 	extsize;
	unsigned	msgsize;
	unsigned 	bytesSent;

	msg = *pmsg;
	actualextsize = pmsg->m_postsize;
	extsize = CA_MESSAGE_ALIGN(pmsg->m_postsize);
	msg.m_postsize = htons((ca_uint16_t)extsize);
	msgsize = extsize+sizeof(msg);


	LOCK;

	/*
	 * 	Force contiguous header and body 
	 *
	 * o	Forces hdr and bdy into the same frame if UDP
	 *
	 * o	Does not allow interleaved messages
	 *	caused by removing recv backlog during 
	 *	send under UNIX.
	 *
	 * o	Does not allow for messages larger than the
	 *	ring buffer size.
	 */
	if(msgsize>piiu->send.max_msg){
		return ECA_TOLARGE;
	}

	bytesAvailable = cacRingBufferWriteSize(&piiu->send, contig);

	if (bytesAvailable<msgsize) {
		UNLOCK;
		return ECA_SERVBEHIND;
	}

	/*
	 * push the header onto the ring 
	 */
	bytesSent = cacRingBufferWrite(
				&piiu->send, 
				&msg, 
				sizeof(msg));
	assert(bytesSent == sizeof(msg));

	/*
	 * push message body onto the ring
	 *
	 * (optionally encode in netrwork format as we send)
	 */
	if (extsize>0u) {
		bytesSent = cacRingBufferWrite(
					&piiu->send, 
					pext, 
					actualextsize);
		assert(bytesSent == actualextsize);
		/*
		 * force pad bytes at the end of the message to nill
		 * if present (this avoids messages from purify)
		 */
		{
			static 		nullBuff[32];
			unsigned	n;

			n = extsize-actualextsize;
			if (n) {
				assert(n<=sizeof(nullBuff));
				bytesSent = cacRingBufferWrite(
						&piiu->send, 
						nullBuff, 
						n);
				assert(bytesSent == n);
			}
		}
	}

	UNLOCK;

	return ECA_NORMAL;
}


/*
 *
 * 	cac_alloc_msg_no_flush()
 *
 *	return a pointer to reserved message buffer space or
 *	nill if the message will not fit
 *
 *	dont flush output if the message does not fit
 *
 *	LOCK should be on
 *
 */ 
LOCAL int cac_alloc_msg_no_flush(
struct ioc_in_use 	*piiu,
unsigned		extsize,
caHdr			**ppMsg
)
{
	unsigned 	msgsize;
	unsigned long	bytesAvailable;
	caHdr		*pmsg;

	/*
	 * 	This only works with UDP (because TCP must be allowed
	 * 	to wrap around from the end to the beg of the buffer)
	 */
	assert (piiu->sock_proto == IPPROTO_UDP);

	msgsize = sizeof(caHdr)+extsize;

	/*
	 * fail if max message size exceeded
	 */
	if(msgsize>=piiu->send.max_msg){
		return ECA_TOLARGE;
	}

	bytesAvailable = cacRingBufferWriteSize (&piiu->send, TRUE);
  	if (bytesAvailable<msgsize) {
		return ECA_TOLARGE;
	}

	pmsg = (caHdr *) &piiu->send.buf[piiu->send.wtix];
	pmsg->m_postsize = extsize;
	*ppMsg = pmsg;

	return ECA_NORMAL;
}


/*
 * cac_add_msg ()
 */
LOCAL void cac_add_msg (IIU *piiu)
{
	unsigned long	size; 
	caHdr 	*mp = EXTMSGPTR(piiu); 

	/*
	 * Performs worst case message alignment
	 */
	mp->m_postsize = (unsigned short) 
		CA_MESSAGE_ALIGN(mp->m_postsize); 
	size = mp->m_postsize;
	mp->m_postsize = htons(mp->m_postsize); 
	CAC_RING_BUFFER_WRITE_ADVANCE( 
		&piiu->send, 
		sizeof(caHdr) + size); 
}



/*
 *	CA_TASK_INITIALIZE
 */
int epicsShareAPI ca_task_initialize(void)
{
	int			status;
	struct CA_STATIC	*ca_temp;

	if (!ca_static) {
		ca_temp = (struct CA_STATIC *) 
			calloc(1, sizeof(*ca_temp));
		if (!ca_temp) {
			return ECA_ALLOCMEM;
		}
		status = cac_os_depen_init (ca_temp);
		return status;
	}

	return ECA_NORMAL;
}


/*
 *	ca_os_independent_init ()
 */
int ca_os_independent_init (void)
{
	long		status;

	installSigPipeIgnore();

	ca_static->ca_exception_func = ca_default_exception_handler;
	ca_static->ca_exception_arg = NULL;

	caSetDefaultPrintfHandler();

	/* record a default user name */
	ca_static->ca_pUserName = localUserName();
	if(!ca_static->ca_pUserName){
		free(ca_static);
		return ECA_ALLOCMEM;
	}

	/* record a default user name */
	ca_static->ca_pHostName = localHostName();
	if(!ca_static->ca_pHostName){
		free(ca_static->ca_pUserName);
		free(ca_static);
		return ECA_ALLOCMEM;
	}

	cac_gettimeval (&ca_static->currentTime);
	ca_static->programBeginTime = ca_static->currentTime;

	/* init sync group facility */
	ca_sg_init();

	/*
	 * init broadcasted search counters
	 * (current time must be initialized before calling this)
	 */
	ca_static->ca_search_responses = 0u;
	ca_static->ca_search_tries = 0u;
	ca_static->ca_search_retry_seq_no = 0u;
	ca_static->ca_seq_no_at_list_begin = 0u;
	ca_static->ca_frames_per_try = TRIESPERFRAME;
	ca_static->ca_conn_next_retry = ca_static->currentTime;
	cacSetRetryInterval (0u);

	ellInit(&ca_static->ca_iiuList);
	ellInit(&ca_static->ca_ioeventlist);
	ellInit(&ca_static->ca_pend_read_list);
	ellInit(&ca_static->ca_pend_write_list);
	ellInit(&ca_static->putCvrtBuf);
	ellInit(&ca_static->fdInfoFreeList);
	ellInit(&ca_static->fdInfoList);

	freeListInitPvt(&ca_static->ca_ioBlockFreeListPVT, 
				sizeof(struct pending_event), 256);

	ca_static->ca_pSlowBucket = 
		bucketCreate(CLIENT_HASH_TBL_SIZE);
	assert(ca_static->ca_pSlowBucket);

	ca_static->ca_pFastBucket = 
		bucketCreate(CLIENT_HASH_TBL_SIZE);
	assert(ca_static->ca_pFastBucket);

	status = envGetDoubleConfigParam (
			&EPICS_CA_CONN_TMO, 
			&ca_static->ca_connectTMO);
	if (status) {
		ca_static->ca_connectTMO = 
			CA_CONN_VERIFY_PERIOD;
		ca_printf (
			"EPICS \"%s\" float fetch failed\n",
			EPICS_CA_CONN_TMO.name);
		ca_printf (
			"Setting \"%s\" = %f\n",
			EPICS_CA_CONN_TMO.name,
			ca_static->ca_connectTMO);
	}

	ca_static->ca_repeater_port = 
		caFetchPortConfig(&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);

	ca_static->ca_server_port = 
		caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

	if (repeater_installed()==FALSE) {
		ca_spawn_repeater();
	}

	ca_static->ca_flush_pending = FALSE;
	ca_static->ca_min_retry = UINT_MAX;
	ca_static->ca_number_iiu_in_fc = 0u;

	return ECA_NORMAL;
}


/*
 * create_udp_fd
 */
void cac_create_udp_fd()
{
	int	status;

	if(ca_static->ca_piiuCast){
		return;
	}

	status = create_net_chan(
			&ca_static->ca_piiuCast,
			NULL,
			IPPROTO_UDP);
	if (~status & CA_M_SUCCESS) {
		ca_static->ca_piiuCast = NULL;
		return;
	}

#ifdef vxWorks
	{
		int     pri;
		char	name[64];

		status = taskPriorityGet(VXTASKIDSELF, &pri);
		if(status<0)
			genLocalExcep (ECA_INTERNAL,NULL);

		strcpy(name,"RD ");
		strncat(
			name,
			taskName(VXTHISTASKID),
			sizeof(name)-strlen(name)-1);

		status = taskSpawn(
					name,
					pri+1,
					VX_FP_TASK,
					4096,
					(FUNCPTR)cac_recv_task,
					(int)taskIdCurrent,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0);
		if (status==ERROR) {
			genLocalExcep (ECA_INTERNAL,NULL);
		}

		ca_static->recv_tid = status;

	}
#endif
}


/*
 * CA_MODIFY_HOST_NAME()
 *
 * Modify or override the default 
 * client host name.
 *
 * This entry point was changed to a NOOP 
 */
int epicsShareAPI ca_modify_host_name(const char *pHostName)
{
	return ECA_NORMAL;
}



/*
 * CA_MODIFY_USER_NAME()
 *
 * Modify or override the default 
 * client user name.
 *
 * This entry point was changed to a NOOP 
 */
int epicsShareAPI ca_modify_user_name(const char *pClientName)
{
	return ECA_NORMAL;
}



/*
 *	CA_TASK_EXIT()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
int epicsShareAPI ca_task_exit (void)
{
	if (!ca_static) {
		return ECA_NOCACTX;
	}

	/*
	 * This indirectly calls ca_process_exit() below
	 */
	cac_os_depen_exit(ca_static);

  	return ECA_NORMAL;
}



/*
 *
 *	CA_TASK_EXIT_TID() / CA_PROCESS_EXIT()
 *	releases all resources alloc to a channel access client
 *	
 *	On multi thread os it is assumed that all threads are 
 *	before calling this routine.
 *
 */
void ca_process_exit()
{
	chid            	chix;
	chid            	chixNext;
	IIU			*piiu;
	int             	status;

	assert(ca_static);

	LOCK;

	/*
	 * after activity eliminated
	 * close all sockets before clearing chid blocks and remote
	 * event blocks
	 */
	piiu = (struct ioc_in_use *)
		ca_static->ca_iiuList.node.next;
	while(piiu){
		if(ca_static->ca_fd_register_func){
			(*ca_static->ca_fd_register_func)(
				(void *)ca_static->ca_fd_register_arg,
				piiu->sock_chan,
				FALSE);
		}
		if (socket_close(piiu->sock_chan) < 0){
			genLocalExcep ( ECA_INTERNAL, 
				"Corrupt iiu list- at close");
		}
		piiu = (struct ioc_in_use *) piiu->node.next;
	}

	/*
	 * remove remote chid blocks and event blocks
	 */
	piiu = (struct ioc_in_use *)
		ca_static->ca_iiuList.node.next;
	while(piiu){
		chix = (chid) ellFirst(&piiu->chidlist);
		while (chix) {
			chixNext = (chid) ellNext (&chix->node);
			clearChannelResources (chix->cid);
			chix = chixNext;
		}

		/*
		 * free message body cache
		 */
		if(piiu->pCurData){
			free(piiu->pCurData);
			piiu->pCurData = NULL;
			piiu->curDataMax = 0;
		}

		/*
		 * free address list
		 */
		ellFree(&piiu->destAddr);

		piiu = (struct ioc_in_use *) piiu->node.next;
	}

	/* remove any pending read blocks */
	caIOBlockListFree (&ca_static->ca_pend_read_list, NULL, FALSE, ECA_INTERNAL);

	/* remove any pending write blocks */
	caIOBlockListFree (&ca_static->ca_pend_write_list, NULL, FALSE, ECA_INTERNAL);

	/* remove any pending io event blocks */
	ellFree(&ca_static->ca_ioeventlist);

	/* remove put convert block free list */
	ellFree(&ca_static->putCvrtBuf);

	/* reclaim sync group resources */
	ca_sg_shutdown(ca_static);

	/* remove remote waiting ev blocks */
	freeListCleanup(ca_static->ca_ioBlockFreeListPVT);

	/* free select context lists */
	ellFree(&ca_static->fdInfoFreeList);
	ellFree(&ca_static->fdInfoList);

	/*
	 * remove IOCs in use
	 */
	ellFree(&ca_static->ca_iiuList);

	/*
	 * free user name string
	 */
	if(ca_static->ca_pUserName){
		free(ca_static->ca_pUserName);
	}

	/*
	 * free host name string
	 */
	if(ca_static->ca_pHostName){
		free(ca_static->ca_pHostName);
	}

	/*
	 * free hash tables 
	 */
	status = bucketFree (ca_static->ca_pSlowBucket);
	assert (status == S_bucket_success);
	status = bucketFree (ca_static->ca_pFastBucket);
	assert (status == S_bucket_success);

	/*
	 * free beacon hash table
	 */
	freeBeaconHash(ca_static);

	UNLOCK;
}


/*
 *
 *      CA_BUILD_AND_CONNECT
 *
 *      backwards compatible entry point to ca_search_and_connect()
 */
int epicsShareAPI ca_build_and_connect
(
const char *name_str,
chtype get_type,
unsigned long get_count,
chid * chixptr,
void *pvalue,
void (*conn_func) (struct connection_handler_args),
void *puser
)
{
        if(get_type != TYPENOTCONN && pvalue!=0 && get_count!=0){
                return ECA_ANACHRONISM;
        }
 
        return ca_search_and_connect(name_str,chixptr,conn_func,puser);
}
 


/*
 *	ca_search_and_connect()	
 */
int epicsShareAPI ca_search_and_connect
(
	const char *name_str,
 	chid *chixptr,
 	void (*conn_func) (struct connection_handler_args),
 	const void *puser
)
{
	long    status;
	ciu	chix;
	int     strcnt;

	/*
	 * make sure that chix is NULL on fail
	 */
	*chixptr = NULL;

	INITCHK;

	/* Put some reasonable limit on user supplied string size */
	strcnt = strlen(name_str) + 1;
	if (strcnt > MAX_STRING_SIZE)
		return ECA_STRTOBIG;
	if (strcnt == 1)
		return ECA_EMPTYSTR;

	/* 
	 * only for IOCs 
	 */
#ifdef vxWorks
	{
		struct dbAddr  tmp_paddr;

		/* Find out quickly if channel is on this node  */
		status = db_name_to_addr(name_str, &tmp_paddr);
		if (status == OK) {
			int size;

			/*
			 * allocate CHIU (channel in use) block
			 *
			 * also allocate enough for the channel name & paddr
			 * block
			 */
			size = CA_MESSAGE_ALIGN(sizeof(*chix) + strcnt) +
				 sizeof(struct dbAddr);
			*chixptr = chix = (ciu) calloc(1,size);
			if (!chix){
				return ECA_ALLOCMEM;
			}
			chix->id.paddr = (struct dbAddr *) 
			(CA_MESSAGE_ALIGN(sizeof(*chix)+strcnt) + (char *)chix);
			*chix->id.paddr = tmp_paddr;
			chix->puser = puser;
			chix->pConnFunc = conn_func;
			chix->type = chix->id.paddr->dbr_field_type;
			chix->count = chix->id.paddr->no_elements;
			chix->piiu = NULL; /* none */
			chix->state = cs_conn;
			chix->ar.read_access = TRUE;
			chix->ar.write_access = TRUE;
			chix->retry = 0;
			ellInit(&chix->eventq);
			strncpy((char *)(chix + 1), name_str, strcnt);

			LOCK;
			ellAdd(&local_chidlist, &chix->node);

			if (chix->pConnFunc) {
				struct connection_handler_args 	args;

				args.chid = chix;
				args.op = CA_OP_CONN_UP;
				(*chix->pConnFunc) (args);
			}
			if (chix->pAccessRightsFunc) {
				struct access_rights_handler_args 	args;

				args.chid = chix;
				args.ar = chix->ar;
				(*chix->pAccessRightsFunc) (args);
			}
			UNLOCK;
			return ECA_NORMAL;
		}
	}
#endif

	if (!ca_static->ca_piiuCast) {
		cac_create_udp_fd();
		if(!ca_static->ca_piiuCast){
			return ECA_NOCAST;
		}
	}

	/* allocate CHIU (channel in use) block */
	/* also allocate enough for the channel name */
	*chixptr = chix = (ciu) calloc(1, sizeof(*chix) + strcnt);
	if (!chix){
		return ECA_ALLOCMEM;
	}

	LOCK;
	do {
		chix->cid = CLIENT_SLOW_ID_ALLOC;
		status = bucketAddItemUnsignedId (pSlowBucket, 
					&chix->cid, chix);
	} while (status == S_bucket_idInUse);

	if (status != S_bucket_success) {
		*chixptr = (chid) NULL;
		free((char *) chix);
		UNLOCK;
		if (status == S_bucket_noMemory) {
			return ECA_ALLOCMEM;
		}
		return ECA_INTERNAL;
	}

	chix->puser = (void *) puser;
	chix->pConnFunc = conn_func;
	chix->type = TYPENOTCONN; /* invalid initial type 	 */
	chix->count = 0; 	/* invalid initial count	 */
	chix->id.sid = ~0U;	/* invalid initial server id 	 */
	chix->ar.read_access = FALSE;
	chix->ar.write_access = FALSE;

	chix->state = cs_never_conn;
	ellInit(&chix->eventq);

	/* Save this channels name for retry if required */
	strncpy((char *)(chix + 1), name_str, strcnt);

	addToChanList(chix, piiuCast);

	/*
	 * reset broadcasted search counters
	 */
	ca_static->ca_conn_next_retry = ca_static->currentTime;
	cacSetRetryInterval (0u);

	/*
	 * Connection Management takes care 
	 * of sending the the search requests
	 */
	if (!chix->pConnFunc) {
		SETPENDRECV;
	}
	UNLOCK;

	return ECA_NORMAL;
}


/*
 * SEARCH_MSG()
 */
int search_msg(
ciu	chix,
int	reply_type
)
{
	int			status;
	int    			size;
	caHdr			*mptr;
	struct ioc_in_use	*piiu;

	piiu = chix->piiu;

	if (piiu!=piiuCast) {
		return ECA_INTERNAL;
	}

	size = strlen((char *)(chix+1))+1;

	LOCK;
	status = cac_alloc_msg_no_flush (piiu, size, &mptr);
	if (status != ECA_NORMAL) {
		UNLOCK;
		return status;
	}

	mptr->m_cmmd = htons (CA_PROTO_SEARCH);
	mptr->m_available = chix->cid;
	mptr->m_type = reply_type;
	mptr->m_count = htons (CA_MINOR_VERSION);
	mptr->m_cid = chix->cid;

	/* 
	 * channel name string - forces a NULL at the end because 
	 * strcnt is always >= strlen + 1 
	 */
	mptr++;
	strncpy((char *)mptr, (char *)(chix + 1), size);

	cac_add_msg(piiu);

	/*
	 * increment the number of times we have tried this
	 * channel
	 */
	if (chix->retry<MAXCONNTRIES) {
		chix->retry++;
	}
	if (ca_static->ca_search_tries<ULONG_MAX) {
		ca_static->ca_search_tries++;
	}

	/*
	 * move the channel to the end of the list so
	 * that all channels get a equal chance 
	 */
	ellDelete(&piiu->chidlist, &chix->node);
	ellAdd(&piiu->chidlist, &chix->node);

	UNLOCK;

	return ECA_NORMAL;
}



/*
 * CA_ARRAY_GET()
 * 
 * 
 */
int epicsShareAPI ca_array_get
(
chtype 		type,
unsigned long 	count,
chid 		chix,
void 		*pvalue
)
{
	int	status;
	miu	monix;

	CHIXCHK(chix);

	if (INVALID_DB_REQ(type))
		return ECA_BADTYPE;

	if(!chix->ar.read_access){
		return ECA_NORDACCESS;
	}

	if (count > chix->count) {
		return ECA_BADCOUNT;
	}

#ifdef vxWorks
	{
		int             status;

		if(!chix->piiu) {
			status = db_get_field(
					      chix->id.paddr,
					      type,
					      pvalue,
					      count,
					      NULL);
			if (status == OK)
				return ECA_NORMAL;
			else
				return ECA_GETFAIL;
		}
	}
#endif

	/*
	 * lock around io block create and list add
	 * so that we are not deleted without
	 * reclaiming the resource
	 */
	LOCK;
	monix = caIOBlockCreate();
	if (monix) {

		monix->chan = chix;
		monix->type = type;
		monix->count = count;
		monix->usr_arg = pvalue;

		ellAdd(&pend_read_list, &monix->node);
	}

	if (monix) {
		status = issue_get_callback(monix, CA_PROTO_READ);
		if (status == ECA_NORMAL) {
			SETPENDRECV;
		}
		else {
			if (ca_state(chix)==cs_conn) {
				ellDelete (&pend_read_list, &monix->node);
				caIOBlockFree (monix);
			}
		}
	} 
	else {
		status = ECA_ALLOCMEM;
	}
	UNLOCK;
	return status;
}



/*
 * CA_ARRAY_GET_CALLBACK()
 */
int epicsShareAPI ca_array_get_callback
(
chtype type,
unsigned long count,
chid chix,
void (*pfunc) (struct event_handler_args),
const void *arg
)
{
	int	status;
	miu	monix;

	CHIXCHK(chix);

	if (INVALID_DB_REQ(type))
		return ECA_BADTYPE;

	if (count > chix->count)
		return ECA_BADCOUNT;

	if(!chix->ar.read_access){
		return ECA_NORDACCESS;
	}

#ifdef vxWorks
	if (!chix->piiu) {
		struct pending_event ev;

		ev.usr_func = pfunc;
		ev.usr_arg = arg;
		ev.chan = chix;
		ev.type = type;
		ev.count = count;
		ca_event_handler(&ev, chix->id.paddr, 0, NULL);
		return ECA_NORMAL;
	}
#endif

	/*
	 * lock around io block create and list add
	 * so that we are not deleted without
	 * reclaiming the resource
	 */
	LOCK;
	monix = caIOBlockCreate();
	if (monix) {

		monix->chan = chix;
		monix->usr_func = pfunc;
		monix->usr_arg = (void *) arg;
		monix->type = type;
		monix->count = count;

		ellAdd(&pend_read_list, &monix->node);
	}

	if (monix) {
		status = issue_get_callback (monix, CA_PROTO_READ_NOTIFY);
		if (status != ECA_NORMAL) {
			if (ca_state(chix)==cs_conn) {
				ellDelete (&pend_read_list, &monix->node);
				caIOBlockFree (monix);
			}
		}
	} 
	else {
		status = ECA_ALLOCMEM;
	}
	UNLOCK;

	return status;
}


/*
 * caIOBlockCreate()
 */
LOCAL miu caIOBlockCreate(void)
{
	int	status;
	miu	pIOBlock;

	LOCK;

	pIOBlock = (miu) freeListCalloc(ca_static->ca_ioBlockFreeListPVT);
	if (pIOBlock) {
		do {
			pIOBlock->id = CLIENT_FAST_ID_ALLOC;
			status = bucketAddItemUnsignedId(
					pFastBucket, 
					&pIOBlock->id, 
					pIOBlock);
		} while (status == S_bucket_idInUse);

		if(status != S_bucket_success){
			freeListFree(ca_static->ca_ioBlockFreeListPVT, 
				pIOBlock);
			pIOBlock = NULL;
		}
	}

	UNLOCK;

	return pIOBlock;
}


/*
 * caIOBlockFree()
 */
void caIOBlockFree(miu pIOBlock)
{
	int	status;

	LOCK;
	status = bucketRemoveItemUnsignedId(
			ca_static->ca_pFastBucket, 
			&pIOBlock->id);
	assert (status == S_bucket_success);
	pIOBlock->id = ~0U; /* this id always invalid */
	freeListFree(ca_static->ca_ioBlockFreeListPVT, pIOBlock);
	UNLOCK;
}


/*
 * Free all io blocks on the list attached to the specified channel
 */
void caIOBlockListFree(
ELLLIST *pList, 
chid 	chan, 
int 	cbRequired, 
int 	status)
{
	miu				monix;
	miu				next;
	struct event_handler_args	args;

	for (monix = (miu) ellFirst (pList);
	     monix;
	     monix = next) {

		next = (miu) ellNext (&monix->node);

		if (chan == NULL || monix->chan == chan) {

			ellDelete (pList, &monix->node);

			args.usr = 	(void *) monix->usr_arg;
			args.chid = 	monix->chan;
			args.type = 	monix->type;
			args.count = 	monix->count;
			args.status = 	status;
			args.dbr = 	NULL;

			caIOBlockFree (monix);

			if (cbRequired && monix->usr_func) {
				(*monix->usr_func) (args);
			}
		}
	}
}


/*
 *
 *	ISSUE_GET_CALLBACK()
 *	(lock must be on)
 */
LOCAL int issue_get_callback(evid monix, unsigned cmmd)
{
	int			status;
	chid   			chix = monix->chan;
	unsigned        	count;
	caHdr		hdr;
	struct ioc_in_use	*piiu;

	piiu = chix->piiu;

  	/* 
	 * dont send the message if the conn is down 
	 * (it will be sent once connected)
  	 */
	if (chix->state != cs_conn) {
		return ECA_BADCHID;
	}

	/*
	 * set to the native count if they specify zero
	 */
	if (monix->count == 0 || monix->count > chix->count){
		count = chix->count;
	}
	else{
		count = monix->count;
	}
	
	/* msg header only on db read notify req	 */
	hdr.m_cmmd = htons (cmmd);
	hdr.m_type = htons (monix->type);
	hdr.m_count = htons (count);
	hdr.m_available = monix->id;
	hdr.m_postsize = 0;
	hdr.m_cid = chix->id.sid;

	status = cac_push_msg (piiu, &hdr, 0);

	return status;
}


/*
 *	CA_ARRAY_PUT_CALLBACK()
 *
 */
int epicsShareAPI ca_array_put_callback
(
chtype				type,
unsigned long			count,
chid				chix,
const void			*pvalue,
void				(*pfunc)(struct event_handler_args),
const void			*usrarg
)
{
	IIU	*piiu;
	int	status;
	miu 	monix;

	/*
	 * valid channel id test
	 */
  	CHIXCHK(chix);

	/*
	 * compound types not allowed
	 */
  	if(dbr_value_offset[type]){
    		return ECA_BADTYPE;
	}

	if(!chix->ar.write_access){
		return ECA_NOWTACCESS;
	}

	/*
	 * check for valid count
	 */
  	if(count > chix->count || count == 0)
    		return ECA_BADCOUNT;

	piiu = chix->piiu;

	if (piiu) {
		if(!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
			return ECA_NOSUPPORT;
		}
	}

	if (type==DBR_STRING) {
		int status;
		status = check_a_dbr_string(pvalue, count);
		if (status != ECA_NORMAL) {
			return status;
		}
	}

#ifdef vxWorks
	if (!piiu) {
		/* cast removes const */
		ciu			pChan = (ciu) chix; 
		CACLIENTPUTNOTIFY	*ppn;
		unsigned		size;

		size = dbr_size_n(type,count);
		LOCK;

		ppn = pChan->ppn;
		if(ppn){
			/*
			 * wait while it is busy
			 */
			if(ppn->busy){
				UNLOCK;
				status = semTake(
					ca_static->ca_blockSem,
					sysClkRateGet()*60);
				if(status != OK){
					return ECA_PUTCBINPROG;
				}
				LOCK;
			}

			/*
		 	 * if not busy then free the current
			 * block if it is to small
			 */
			if(ppn->valueSize<size){
				free(ppn);
				ppn = pChan->ppn = NULL;
			}
		}

		if(!ppn){
			ppn = (CACLIENTPUTNOTIFY *) 
				calloc(1, sizeof(*ppn)+size);
			if(!ppn){
				UNLOCK;
				return ECA_ALLOCMEM;
			}
			pChan->ppn = ppn;
			ppn->pcas = ca_static;
			ppn->dbPutNotify.userCallback = 
				ca_put_notify_action;
			ppn->dbPutNotify.usrPvt = pChan;
			ppn->dbPutNotify.paddr = chix->id.paddr;
			ppn->dbPutNotify.pbuffer = (ppn+1);
		}
		ppn->busy = TRUE;
		ppn->caUserCallback = pfunc;
		ppn->caUserArg = (void *) usrarg;
		ppn->dbPutNotify.nRequest = count;
		memcpy(ppn->dbPutNotify.pbuffer, (char *)pvalue, size);
		status = dbPutNotifyMapType(&ppn->dbPutNotify, type);
		if(status){
			UNLOCK;
			ppn->busy = FALSE;
			return ECA_PUTFAIL;
		}
		status = dbPutNotify(&ppn->dbPutNotify);
		UNLOCK;
		if(status && status != S_db_Pending){
			if(status==S_db_Blocked){
				return ECA_PUTCBINPROG;
			}
			ppn->busy = FALSE;
			return ECA_PUTFAIL;
		}
		return ECA_NORMAL;
	}
#endif /*vxWorks*/

	/*
	 * lock around io block create and list add
	 * so that we are not deleted without
	 * reclaiming the resource
	 */
	LOCK;
	monix = caIOBlockCreate();
	if (!monix) {
		UNLOCK;
		return ECA_ALLOCMEM;
	}
	ellAdd(&pend_write_list, &monix->node);
	UNLOCK;

	monix->chan = chix;
	monix->usr_func = pfunc;
	monix->usr_arg = (void *) usrarg;
	monix->type = type;
	monix->count = count;

	status = issue_ca_array_put(
			CA_PROTO_WRITE_NOTIFY, 
			monix->id,
			type, 
			count, 
			chix, 
			pvalue);
	if(status != ECA_NORMAL){
		LOCK;
		if (ca_state(chix)==cs_conn) {
			ellDelete (&pend_write_list, &monix->node);
			caIOBlockFree(monix);
		}
		UNLOCK;
		return status;
	}


  	return status;
}


/*
 * 	CA_PUT_NOTIFY_ACTION
 */
#ifdef vxWorks
LOCAL void ca_put_notify_action(PUTNOTIFY *ppn)
{
	CACLIENTPUTNOTIFY	*pcapn;
	struct ioc_in_use	*piiu;
	struct CA_STATIC	*pcas;
	chid			chix;

	/*
	 * we choose to suspend the task if there
	 * is an internal failure
	 */
	chix = (chid) ppn->usrPvt;
	if(!chix){
		taskSuspend(0);
	}
	if(!chix->ppn){
		taskSuspend(0);
	}

	piiu = chix->piiu;
	pcapn = chix->ppn;
	pcas = pcapn->pcas;

        /*
         * independent lock used here in order to
         * avoid any possibility of blocking
         * the database (or indirectly blocking
         * one client on another client).
         */
        semTake(pcas->ca_putNotifyLock, WAIT_FOREVER);
        ellAdd(&pcas->ca_putNotifyQue, &pcapn->node);
        semGive(pcas->ca_putNotifyLock);

        /*
         * offload the labor for this to the
         * event task so that we never block
         * the db or another client.
         */
        db_post_extra_labor(pcas->ca_evuser);

}
#endif /*vxWorks*/



/*
 *	CA_ARRAY_PUT()
 *
 *
 */
int epicsShareAPI ca_array_put (
	chtype			type,
	unsigned long		count,
	chid			chix,
	const void		*pvalue
)
{
	/*
	 * valid channel id test
	 */
  	CHIXCHK(chix);

	/*
	 * compound types not allowed
	 */
  	if(dbr_value_offset[type]){
    		return ECA_BADTYPE;
	}

	if(!chix->ar.write_access){
		return ECA_NOWTACCESS;
	}

	/*
	 * check for valid count
	 */
  	if(count > chix->count || count == 0)
    		return ECA_BADCOUNT;

	if (type==DBR_STRING) {
		int status;
		status = check_a_dbr_string(pvalue, count);
		if (status != ECA_NORMAL) {
			return status;
		}
	}

#ifdef vxWorks
	/*
	 * If channel is on this client's host then
	 * call the database directly
	 */
  	if(!chix->piiu){
    		int status;

      		status = db_put_field(  chix->id.paddr,  
                               		type,   
                               		pvalue,
                               		count);            
      		if(status==OK)
			return ECA_NORMAL;
      		else
        		return ECA_PUTFAIL;
  	}
#endif /*vxWorks*/

	return issue_ca_array_put(CA_PROTO_WRITE, ~0U, type, count, chix, pvalue);
}

/*
 * check_a_dbr_string()
 */
LOCAL int check_a_dbr_string(const char *pStr, const unsigned count)
{
	unsigned i;

	for (i=0; i< count; i++) {
		unsigned int strsize;

		strsize = strlen(pStr) + 1;

		if (strsize>MAX_STRING_SIZE) {
			return ECA_STRTOBIG;
		}

		pStr += MAX_STRING_SIZE;
	}

	return ECA_NORMAL;
}



/*
 * issue_ca_array_put()
 */
LOCAL int issue_ca_array_put
(
unsigned	cmd,
unsigned	id,
chtype		type,
unsigned long	count,
chid		chix,
const void	*pvalue
)
{ 
	int			status;
	struct ioc_in_use	*piiu;
	caHdr		hdr;
  	int  			postcnt;
  	unsigned		size_of_one;
#	ifdef CONVERSION_REQUIRED 
  	unsigned		i;
	void			*pCvrtBuf;
  	void			*pdest;
#	endif /*CONVERSION_REQUIRED*/

	piiu = chix->piiu;
	size_of_one = dbr_size[type];
	postcnt = dbr_size_n(type,count);

	if(type == DBR_STRING && count == 1){
		char *pstr = (char *)pvalue;

		postcnt = strlen(pstr)+1;
	}

#	ifdef CONVERSION_REQUIRED 
	pCvrtBuf = pdest = malloc_put_convert(postcnt);
	if(!pdest){
		return ECA_ALLOCMEM;
	}

	/*
	 * No compound types here because these types are read only
	 * and therefore only appropriate for gets or monitors
	 */
    	for(i=0; i< count; i++){
      		switch(type){
      		case	DBR_LONG:
        		*(long *)pdest = htonl(*(long *)pvalue);
        		break;

      		case	DBR_CHAR:
        		*(char *)pdest = *(char *)pvalue;
        		break;

      		case	DBR_ENUM:
      		case	DBR_SHORT:
		case	DBR_PUT_ACKT:
		case	DBR_PUT_ACKS:
#		if DBR_INT != DBR_SHORT
      		case	DBR_INT:
#		endif /*DBR_INT != DBR_SHORT*/
        		*(short *)pdest = htons(*(short *)pvalue);
        		break;

      		case	DBR_FLOAT:
        		dbr_htonf(pvalue, pdest);
        		break;

      		case	DBR_DOUBLE: 
        		dbr_htond(pvalue, pdest);
			break;

      		case	DBR_STRING:
			/*
			 * string size checked above
			 */
          		strcpy(pdest,pvalue);
        		break;

      		default:
			UNLOCK;
        		return ECA_BADTYPE;
      		}
      		pdest = ((char *)pdest) + size_of_one;
      		pvalue = ((char *)pvalue) + size_of_one;
    	}

	pvalue = pCvrtBuf;

#	endif /*CONVERSION_REQUIRED*/

    	hdr.m_cmmd 		= htons(cmd);
   	hdr.m_type		= htons(type);
    	hdr.m_count 		= htons((ca_uint16_t)count);
    	hdr.m_cid 		= chix->id.sid;
    	hdr.m_available 	= id;
	hdr.m_postsize 		= (ca_uint16_t) postcnt;

	status = cac_push_msg(piiu, &hdr, pvalue);

#	ifdef CONVERSION_REQUIRED
	free_put_convert(pCvrtBuf);
#	endif /*CONVERSION_REQUIRED*/

  	return status;
}


/*
 * malloc_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void *malloc_put_convert(unsigned long size)
{
	struct putCvrtBuf	*pBuf;

	LOCK;
	pBuf = (struct putCvrtBuf *) ellFirst(&ca_static->putCvrtBuf);
	while(pBuf){
		if(pBuf->size >= size){
			break;
		}
		pBuf = (struct putCvrtBuf *) ellNext(&pBuf->node);
	}

	if(pBuf){
		ellDelete(&ca_static->putCvrtBuf, &pBuf->node);
	}
	UNLOCK;

	if(!pBuf){
		pBuf = (struct putCvrtBuf *) malloc(sizeof(*pBuf)+size);
		if(!pBuf){
			return NULL;
		}
		pBuf->size = size;
		pBuf->pBuf = (void *) (pBuf+1);
	}

	return pBuf->pBuf;
}
#endif /* CONVERSION_REQUIRED */


/*
 * free_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void free_put_convert(void *pBuf)
{
	struct putCvrtBuf	*pBufHdr;

	pBufHdr = (struct putCvrtBuf *)pBuf;
	pBufHdr -= 1;
	assert(pBufHdr->pBuf == (void *)(pBufHdr+1));

	LOCK;
	ellAdd(&ca_static->putCvrtBuf, &pBufHdr->node);
	UNLOCK;

	return;
}
#endif /* CONVERSION_REQUIRED */


/*
 *	Specify an event subroutine to be run for connection events
 *
 */
int epicsShareAPI ca_change_connection_event
(
chid		chix,
void 		(*pfunc)(struct connection_handler_args)
)
{
	ciu	pChan = (ciu) chix; /* remove const */

  	INITCHK;
  	LOOSECHIXCHK(pChan);

	if(pChan->pConnFunc == pfunc)
  		return ECA_NORMAL;

  	LOCK;
	if (pChan->state==cs_never_conn) {
		if(!pChan->pConnFunc){
    			CLRPENDRECV;
		}
		if(!pfunc){
    			SETPENDRECV;
		}
	}
  	pChan->pConnFunc = pfunc;
  	UNLOCK;

  	return ECA_NORMAL;
}


/*
 * ca_replace_access_rights_event
 */
int epicsShareAPI ca_replace_access_rights_event(
chid 	chan, 
void    (*pfunc)(struct access_rights_handler_args))
{
	ciu pChan = (ciu) chan; /* remove const */
	struct access_rights_handler_args 	args;

  	INITCHK;
  	LOOSECHIXCHK(pChan);

    	pChan->pAccessRightsFunc = pfunc;

	/*
	 * make certain that it runs at least once
	 */
	if(pChan->state == cs_conn && pChan->pAccessRightsFunc){
		args.chid = chan;
		args.ar = chan->ar;
		(*pChan->pAccessRightsFunc)(args);
	}

  	return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for asynch exceptions
 *
 */
int epicsShareAPI ca_add_exception_event
(
	void 		(*pfunc)(struct exception_handler_args),
	const void 	*arg
)
{

  	INITCHK;
  	LOCK;
  	if (pfunc) {
    		ca_static->ca_exception_func = pfunc;
    		ca_static->ca_exception_arg = arg;
  	}
  	else {
    		ca_static->ca_exception_func = ca_default_exception_handler;
    		ca_static->ca_exception_arg = NULL;
  	}
  	UNLOCK;

  	return ECA_NORMAL;
}


/*
 *	CA_ADD_MASKED_ARRAY_EVENT
 *
 *
 */
int epicsShareAPI ca_add_masked_array_event
(
chtype 		type,
unsigned long	count,
chid 		chix,
void 		(*ast)(struct event_handler_args),
const void 	*astarg,
ca_real		p_delta,
ca_real		n_delta,
ca_real		timeout,
evid 		*monixptr,
long		mask
)
{
	ciu 	pChan = (ciu) chix; /* remove const */
  	miu	monix;
  	int	status;

  	INITCHK;
  	LOOSECHIXCHK(pChan);

  	if(INVALID_DB_REQ(type))
    		return ECA_BADTYPE;

	/*
	 * Check for huge waveform
	 *
	 * (the count is not checked here against the native count
	 * when connected because this introduces a race condition 
	 * for the client tool - the requested count is clipped to 
	 * the actual count when the monitor request is sent so
	 * verifying that the requested count is valid here isnt
	 * required)
	 */
	if(dbr_size_n(type,count)>MAX_MSG_SIZE-sizeof(caHdr)){
		return ECA_TOLARGE;
	}

  	if(!mask)
    		return ECA_BADMASK;

	/*
	 * lock around io block create and list add
	 * so that we are not deleted without
	 * reclaiming the resource
	 */
  	LOCK;

  	if (!pChan->piiu) {
# 		ifdef vxWorks
			monix = freeListMalloc (ca_static->ca_dbMonixFreeList);
# 		else
		return ECA_INTERNAL;
# 			endif
  	}
	else {
		monix = caIOBlockCreate();
	}

  	if(!monix){
    		UNLOCK;
    		return ECA_ALLOCMEM;
  	}

  	/* they dont have to supply one if they dont want to */
  	if(monixptr){
    		*monixptr = monix;
	}

  	monix->chan	= chix;
  	monix->usr_func	= ast;
  	monix->usr_arg	= (void *) astarg;
  	monix->type	= type;
  	monix->count	= count;
  	monix->p_delta	= p_delta;
  	monix->n_delta	= n_delta;
  	monix->timeout	= timeout;
  	monix->mask	= (unsigned short) mask;

# 	ifdef vxWorks
	if(!pChan->piiu){
		status = db_add_event(	
				evuser,
				pChan->id.paddr,
				ca_event_handler,
				monix,
				mask,
				(struct event_block *)(monix+1));
		if(status == ERROR){
			UNLOCK;
			freeListFree (ca_static->ca_dbMonixFreeList, monix);
			return ECA_DBLCLFAIL; 
		}

		/* 
		 * Place in the channel list 
		 * - do it after db_add_event so there
		 * is no chance that it will be deleted 
		 * at exit before it is completely created
		 */
		ellAdd(&pChan->eventq, &monix->node);

		/* 
		 * force event to be called at least once
		 * return warning msg if they have made the queue to full 
		 * to force the first (untriggered) event.
		 */
		if(db_post_single_event((struct event_block *)(monix+1))==ERROR){
			UNLOCK;
			return ECA_OVEVFAIL;
		}

		UNLOCK;
		return ECA_NORMAL;
	} 
# 	endif

  	/* It can be added to the list any place if it is remote */
  	/* Place in the channel list */
  	ellAdd(&pChan->eventq, &monix->node);

  	UNLOCK;

	if(pChan->state == cs_conn){
  		status = ca_request_event(monix);
		if (status != ECA_NORMAL) {
			LOCK;
			if (ca_state(pChan)==cs_conn) {
				ellDelete (&pChan->eventq, &monix->node);
				caIOBlockFree(monix);
			}
			UNLOCK
		}
		return status;
	}
	else{
		return ECA_NORMAL;
	}
}


/*
 *	CA_REQUEST_EVENT()
 */
int ca_request_event(evid monix)
{
	int			status;
	chid   			chix = monix->chan;
	unsigned        	count;
	struct monops		msg;
	struct ioc_in_use	*piiu;
	ca_float32_t		p_delta;
	ca_float32_t		n_delta;
	ca_float32_t		tmo;
	
	piiu = chix->piiu;

  	/* 
	 * dont send the message if the conn is down 
	 * (it will be sent once connected)
  	 */
	if(chix->state != cs_conn){
		return ECA_BADCHID;
	}

	/*
	 * clip to the native count and set to the native count if they
	 * specify zero
	 */
	if (monix->count > chix->count || monix->count == 0){
		count = chix->count;
	}
	else{
		count = monix->count;
	}

	/* msg header	 */
	msg.m_header.m_cmmd = htons(CA_PROTO_EVENT_ADD);
	msg.m_header.m_available = monix->id;
	msg.m_header.m_type = htons(monix->type);
	msg.m_header.m_count = htons(count);
	msg.m_header.m_cid = chix->id.sid;
	msg.m_header.m_postsize = sizeof(msg.m_info);

	/* msg body	 */
	p_delta = (ca_float32_t) monix->p_delta;
	n_delta = (ca_float32_t) monix->n_delta;
	tmo = (ca_float32_t) monix->timeout;
	dbr_htonf(&p_delta, &msg.m_info.m_hval);
	dbr_htonf(&n_delta, &msg.m_info.m_lval);
	dbr_htonf(&tmo, &msg.m_info.m_toval);
	msg.m_info.m_mask = htons(monix->mask);
	msg.m_info.m_pad = 0; /* allow future use */	

	status = cac_push_msg(piiu, &msg.m_header, &msg.m_info);

	return status;
}


/*
 *
 *	CA_EVENT_HANDLER()
 *	(only for local (for now vxWorks) clients)
 *
 */
#ifdef vxWorks
LOCAL void ca_event_handler(
void		*usrArg,
struct dbAddr	*paddr,
int		hold,
db_field_log	*pfl
)
{
	miu monix = (miu) usrArg;
  	int status;
  	int count;
  	union db_access_val valbuf;
  	void *pval;
  	unsigned size;
	struct tmp_buff{
		ELLNODE node;
		unsigned size;
	};
	struct tmp_buff *pbuf = NULL;

  	/*
  	 * clip to the native count
  	 * and set to the native count if they specify zero
  	 */
  	if(monix->count > monix->chan->count || monix->count == 0){
		count = monix->chan->count;
	}
	else{
		count = monix->count;
	}

  	if(monix->type == paddr->dbr_field_type){
    		pval = paddr->pfield;
    		status = OK;
  	}
  	else{
		size = dbr_size_n(monix->type,count);

    		if( size <= sizeof(valbuf) ){
      			pval = (void *) &valbuf;
		}
		else{

			/*
			 * find a preallocated block which fits
			 * (stored with lagest block first)
			 */
			LOCK;
			pbuf = (struct tmp_buff *)
					lcl_buff_list.node.next;
			if(pbuf && pbuf->size >= size){
				ellDelete(
					&lcl_buff_list,
					&pbuf->node);
			}else
				pbuf = NULL;
			UNLOCK;

			/* 
			 * test again so malloc is not inside LOCKED
			 * section
			 */
			if(!pbuf){
				pbuf = (struct tmp_buff *)
					malloc(sizeof(*pbuf)+size);
				if(!pbuf){
					ca_printf("%s: No Mem, Event Discarded\n",
						__FILE__);
					return;
				}
				pbuf->size = size;
			}
			pval = (void *) (pbuf+1);
		}
	}


   	status = db_get_field(	paddr,
				monix->type,
				pval,
				count,
				pfl);

  	/*
	 * Call user's callback
   	 */
	LOCK;
	if (monix->usr_func) {
		struct event_handler_args args;
		
		args.usr = (void *) monix->usr_arg;
		args.chid = monix->chan;
		args.type = monix->type;
		args.count = count;
		args.dbr = pval;

  		if(status == OK){
			args.status = ECA_NORMAL;
		}
		else{
			args.status = ECA_GETFAIL;
		}

   	 	(*monix->usr_func)(args);
	}

	/*
	 *
	 * insert the buffer back into the que in size order if
	 * one was used.
	 */
	if(pbuf){
		struct tmp_buff		*ptbuf;

		for(	ptbuf = (struct tmp_buff *) lcl_buff_list.node.next;
			ptbuf;
			ptbuf = (struct tmp_buff *) pbuf->node.next){

			if(ptbuf->size <= pbuf->size){
				break;
			}
		}
		if(ptbuf)
			ptbuf = (struct tmp_buff *) ptbuf->node.previous;

		ellInsert(	
			&lcl_buff_list,
			&ptbuf->node,
			&pbuf->node);
	}
	UNLOCK;

  	return;
}
#endif



/*
 *
 *	CA_CLEAR_EVENT(MONIX)
 *
 *	Cancel an outstanding event for a channel.
 *
 *	NOTE: returns before operation completes in the server 
 *	(and the client). 
 *	This is a good reason not to allow them to make the monix 
 *	block as part of a larger structure.
 *	Nevertheless the caller is gauranteed that his specified
 *	event is disabled and therefore will not run (from this source)
 *	after leaving this routine.
 *
 */
int epicsShareAPI ca_clear_event (evid monix)
{
	ciu   	chix = (ciu) monix->chan; /* cast removes const */
	miu	pMon = (miu) monix; /* cast removes const */
	int	status;
	caHdr 	hdr;
	evid	lkup;

	/*
	 * is it a valid channel ?
	 */
	LOOSECHIXCHK(chix);

	/*
	 * is it a valid monitor id
	 */
	if (chix->piiu) {
		LOCK;
		lkup = (evid) bucketLookupItemUnsignedId(
					pFastBucket,
					&pMon->id);
		UNLOCK;
		if (lkup != pMon) {
			return ECA_BADMONID;
		}
	}

	/* disable any further events from this event block */
	pMon->usr_func = NULL;

#ifdef vxWorks
	if (!chix->piiu) {
		register int    status;

		/*
		 * dont allow two threads to delete the same moniitor at once
		 */
		LOCK;
		ellDelete(&chix->eventq, &pMon->node);
		status = db_cancel_event((struct event_block *)(pMon + 1));
		UNLOCK;
		freeListFree (ca_static->ca_dbMonixFreeList, pMon);

		return ECA_NORMAL;
	}
#endif /*vxWorks*/

	/*
	 * dont send the message if the conn is down (just delete from the
	 * queue and return)
	 * 
	 * check for conn down while locked to avoid a race
	 */
	if(chix->state == cs_conn){
		struct ioc_in_use 	*piiu;
		
		piiu = chix->piiu;

		/* msg header	 */
		hdr.m_cmmd = htons(CA_PROTO_EVENT_CANCEL);
		hdr.m_available = pMon->id;
		hdr.m_type = htons(chix->type);
		hdr.m_count = htons(chix->count);
		hdr.m_cid = chix->id.sid;
		hdr.m_postsize = 0;
	
		status = cac_push_msg(piiu, &hdr, NULL);

		/* 
		 *	NOTE: I free the monitor block only
		 *	after confirmation from IOC 
		 */
	}
	else{
		LOCK;
		ellDelete(&chix->eventq, &pMon->node);
		caIOBlockFree(pMon);
		UNLOCK;
		status = ECA_NORMAL;
	}

	return status;
}


/*
 *
 *	CA_CLEAR_CHANNEL(CHID)
 *
 *	clear the resources allocated for a channel by search
 *
 *	NOTE: returns before operation completes in the server 
 *	(and the client). 
 *	This is a good reason not to allow them to make the monix 
 *	block part of a larger structure.
 *	Nevertheless the caller is gauranteed that his specified
 *	event is disabled and therefore will not run 
 *	(from this source) after leaving this routine.
 *
 */
int epicsShareAPI ca_clear_channel (chid chix)
{
	ciu 			pChan = (ciu) chix; /* remove const */
	miu			monix;
	int			status;
	struct ioc_in_use 	*piiu = chix->piiu;
	caHdr 			hdr;
	caCh			*pold_ch;
	enum channel_state	old_chan_state;

	LOOSECHIXCHK(pChan);

	/* disable their further use of deallocated channel */
	pChan->type = TYPENOTINUSE;
	old_chan_state = pChan->state;
	pChan->state = cs_closed;
	pChan->pAccessRightsFunc = NULL;
	pold_ch = pChan->pConnFunc;
	pChan->pConnFunc = NULL;

	/* the while is only so I can break to the lock exit */
	LOCK;
	/* disable any further events from this channel */
	for (monix = (miu) pChan->eventq.node.next;
	     monix;
	     monix = (miu) monix->node.next)
		monix->usr_func = NULL;
	/* disable any further get callbacks from this channel */
	for (monix = (miu) pend_read_list.node.next;
	     monix;
	     monix = (miu) monix->node.next)
		if (monix->chan == chix)
			monix->usr_func = NULL;
	/* disable any further put callbacks from this channel */
	for (monix = (miu) pend_write_list.node.next;
	     monix;
	     monix = (miu) monix->node.next)
		if (monix->chan == chix)
			monix->usr_func = NULL;

#ifdef vxWorks
	if (!pChan->piiu) {
		CACLIENTPUTNOTIFY	*ppn;
		int             	status;

		/*
		 * clear out the events for this channel
		 */
		while ( (monix = (miu) ellGet(&pChan->eventq)) ) {
			status = db_cancel_event((struct event_block *)(monix + 1));
			assert (status == OK);
			freeListFree (ca_static->ca_dbMonixFreeList, monix);
		}

		/*
		 * cancel any outstanding put notifies
		 */
		if(pChan->ppn){
			ppn = pChan->ppn;
			if(ppn->busy){
				dbNotifyCancel(&ppn->dbPutNotify);
			}
			free(ppn);
		}

		/*
		 * clear out this channel
		 */
		ellDelete(&local_chidlist, &pChan->node);
		free((char *) pChan);

		UNLOCK;
		return ECA_NORMAL;
	}
#endif

	/*
	 * if this channel does not have a connection handler
	 * and it has not connected for the first time then clear the
	 * outstanding IO count
	 */
	if (old_chan_state == cs_never_conn && !pold_ch) {
		CLRPENDRECV;
	}

	/*
	 * dont send the message if not conn 
	 * (just delete from the queue and return)
	 * 
	 * check for conn state while locked to avoid a race
	 */
	if (old_chan_state != cs_conn) {
		UNLOCK;
		clearChannelResources (pChan->cid);
		return ECA_NORMAL;
	}

	/*
	 * clear events and all other resources for this chid on the
	 * IOC
	 */

	/* msg header	 */
	hdr.m_cmmd = htons(CA_PROTO_CLEAR_CHANNEL);
	hdr.m_available = pChan->cid;
	hdr.m_cid = pChan->id.sid;
	hdr.m_type = htons(0);
	hdr.m_count = htons(0);
	hdr.m_postsize = 0;

	status = cac_push_msg(piiu, &hdr, NULL);

	/*
	 * NOTE: I free the chid and monitor blocks only after
	 * confirmation from IOC
	 */

	UNLOCK;

	return status;
}


/*
 * ca_cidToChid()
 */
chid epicsShareAPI ca_cidToChid(unsigned id)
{
	return (chid) bucketLookupItemUnsignedId(ca_static->ca_pSlowBucket, &id);
}


/*
 * clearChannelResources()
 */
void clearChannelResources(unsigned id)
{
	struct ioc_in_use       *piiu;
	ciu			chix;
	int			status;

	LOCK;

	chix = bucketLookupItemUnsignedId(ca_static->ca_pSlowBucket, &id);
	assert ( chix!=NULL );

	piiu = chix->piiu;

	/*
	 * remove any orphaned get callbacks for this channel
	 */
	caIOBlockListFree (&ca_static->ca_pend_read_list, chix, FALSE, ECA_INTERNAL);

	/*
	 * remove any orphaned put callbacks for this channel
	 */
	caIOBlockListFree (&ca_static->ca_pend_write_list, chix, FALSE, ECA_INTERNAL);

	/*
	 * remove any monitors still attached to this channel
	 */
	caIOBlockListFree (&chix->eventq, NULL, FALSE, ECA_INTERNAL);

	status = bucketRemoveItemUnsignedId (
			ca_static->ca_pSlowBucket, &chix->cid);
	assert (status == S_bucket_success);
	removeFromChanList(chix);
	free (chix);
	if (piiu!=piiuCast && ellCount(&piiu->chidlist.count)==0){
		TAG_CONN_DOWN(piiu);
	}
	
	UNLOCK;
}



/************************************************************************/
/*	This routine pends waiting for channel events and calls the 	*/
/*	timeout is specified as 0 infinite timeout is assumed.		*/
/*	if the argument early is specified TRUE then CA_NORMAL is 	*/
/*	returned early (prior to timeout experation) when outstanding 	*/
/*	IO completes.							*/
/*	Output buffers are flushed by this routine			*/
/************************************************************************/
int epicsShareAPI ca_pend (ca_real timeout, int early)
{
	struct timeval	beg_time;
	ca_real		delay;
	struct timeval	tmo;

  	INITCHK;

	if(EVENTLOCKTEST){
    		return ECA_EVDISALLOW;
	}

	cac_gettimeval (&ca_static->currentTime);

  	/*	
	 * Flush the send buffers
	 * (guarantees that we wait for all send buffer to be 
	 * flushed even if this requires blocking)
	 */
	ca_static->ca_flush_pending = TRUE;

    	if(pndrecvcnt==0u && early){
		/*
		 * force the flush
		 */
		CLR_CA_TIME (&tmo);
		cac_mux_io(&tmo);
        	return ECA_NORMAL;
	}

	if(timeout<0.0){
		/*
		 * force the flush
		 */
		CLR_CA_TIME (&tmo);
		cac_mux_io(&tmo);
		return ECA_TIMEOUT;
	}

	beg_time = ca_static->currentTime;
	delay = 0.0;
  	while(TRUE){
		ca_real 	remaining;

    		if (pndrecvcnt==0 && early) {
			/*
			 * force the flush
			 */
			CLR_CA_TIME (&tmo);
			cac_mux_io(&tmo);
        		return ECA_NORMAL;
		}

    		if(timeout == 0.0){
			remaining = cac_fetch_poll_period();
		}
		else{
			remaining = timeout-delay;
			/*
			 * Allow for CA background labor
			 */
			remaining = min(cac_fetch_poll_period(), remaining);
  		}    


		/*
		 * If we are not waiting for any significant delay
		 * then force the delay to zero so that we avoid
		 * scheduling delays (which can be substantial
		 * on some os)
		 */
#		ifdef CLOCKS_PER_SEC
#			define CAC_SIGNIF_INTERVAL (1.0/CLOCKS_PER_SEC)
#		else
			/*
			 * we guess (because gcc does not provide
			 * CLOCKS_PER_SEC under sunos4)
			 */
#			define CAC_SIGNIF_INTERVAL (1.0/1000000)
#		endif
		if (remaining <= CAC_SIGNIF_INTERVAL) {
			if(early){
				ca_pend_io_cleanup();
				ca_static->ca_flush_pending = TRUE;
			}
			/* 
			 * be certain that we processed
			 * recv backlog at least once
			 */
			/*
			 * force the flush
			 */
			CLR_CA_TIME (&tmo);
			cac_block_for_io_completion (&tmo);
			return ECA_TIMEOUT;
		}
		tmo.tv_sec = (long) remaining;
		tmo.tv_usec = (long) ((remaining-tmo.tv_sec)*USEC_PER_SEC);
		cac_block_for_io_completion (&tmo);

		if (timeout != 0.0) {
			delay = cac_time_diff (&ca_static->currentTime, 
							&beg_time);
		}
	}
}

/*
 * cac_fetch_poll_period()
 */
double cac_fetch_poll_period(void)
{
	if (!piiuCast) {
		return SELECT_POLL_NO_SEARCH;
	}
	else if (ellCount(&piiuCast->chidlist)==0) {
		return SELECT_POLL_NO_SEARCH;
	}
	else {
		return SELECT_POLL_SEARCH;
	}
}


/*
 * cac_time_diff()
 */
ca_real cac_time_diff (ca_time *pTVA, ca_time *pTVB)
{
        ca_real         delay;
        ca_real         udelay;

        /*
         * works with unsigned tv_sec in struct timeval
         * under HPUX
         */
        if (pTVA->tv_sec>pTVB->tv_sec) {
                delay = pTVA->tv_sec - pTVB->tv_sec;
        }
        else {
                delay = pTVB->tv_sec - pTVA->tv_sec;
                delay = -delay;
        }

        if(pTVA->tv_usec>pTVB->tv_usec){
                udelay = pTVA->tv_usec - pTVB->tv_usec;
        }
        else{
                delay -= 1.0;
                udelay = (USEC_PER_SEC - pTVB->tv_usec) + pTVA->tv_usec;
        }
        delay += udelay / USEC_PER_SEC;

        return delay;
}


/*
 * cac_time_sum()
 */
ca_time cac_time_sum (ca_time *pTVA, ca_time *pTVB)
{
	long	usum;
	ca_time	sum;

	sum.tv_sec = pTVA->tv_sec + pTVB->tv_sec;
	usum = pTVA->tv_usec +  pTVB->tv_usec;	
	if(usum>=USEC_PER_SEC){
		sum.tv_sec++;
		sum.tv_usec = usum-USEC_PER_SEC;
	}
	else{
		sum.tv_usec = usum;
	}

	return sum;
}

/*
 * noopConnHandler()
 * This is installed into channels which dont have
 * a connection handler when ca_pend_io() times
 * out so that we will not decrement the pending
 * recv count in the future.
 */
LOCAL void noopConnHandler(struct  connection_handler_args args)
{
}

/*
 *
 * set pending IO count back to zero and
 * send a sync to each IOC and back. dont
 * count reads until we recv the sync
 *
 */
LOCAL void ca_pend_io_cleanup()
{
	struct ioc_in_use *piiu;
	ciu pchan;

	LOCK;
	pchan = (ciu) ellFirst (&piiuCast->chidlist);
	while (pchan) {
		if (!pchan->pConnFunc) {
			pchan->pConnFunc = noopConnHandler;
		}
		pchan = (ciu) ellNext (&pchan->node);
	}
	for(	piiu = (IIU *) iiuList.node.next;
		piiu;
		piiu = (IIU *) piiu->node.next){

		caHdr hdr;

		if (piiu->state!=iiu_connected || piiu==piiuCast) {
			continue;
		}

		piiu->cur_read_seq++;

		hdr = nullmsg;
		hdr.m_cmmd = htons(CA_PROTO_READ_SYNC);
		cac_push_msg(piiu, &hdr, NULL);
	}
	UNLOCK;
	pndrecvcnt = 0u;
}



/*
 *	CA_FLUSH_IO()
 * 	reprocess connection state and
 * 	flush the send buffer 
 */
int epicsShareAPI ca_flush_io()
{
	struct timeval  	timeout;

  	INITCHK;

	/*
	 * Wait for all send buffers to be flushed
	 * while performing socket io and processing recv backlog
	 */
	ca_static->ca_flush_pending = TRUE;
	CLR_CA_TIME (&timeout);
	cac_mux_io (&timeout);

  	return ECA_NORMAL;
}


/*
 *	CA_TEST_IO ()
 *
 */
int epicsShareAPI ca_test_io()
{
    	if(pndrecvcnt==0u){
        	return ECA_IODONE;
	}
	else{
		return ECA_IOINPROGRESS;
	}
}


/*
 * genLocalExcepWFL ()
 * (generate local exception with file and line number)
 */
void genLocalExcepWFL (long stat, char *ctx, char *pFile, unsigned lineNo)
{
	struct exception_handler_args args;

	/*
	* NOOP if they disable exceptions
	*/
	if (!ca_static->ca_exception_func) {
	    return;
	}
 
	args.usr = (void *) ca_static->ca_exception_arg;
	args.chid = NULL;
	args.type = -1;
	args.count = 0u;
	args.addr = NULL;
	args.stat = stat;
	args.op = CA_OP_OTHER;
	args.ctx = ctx;
	args.pFile = pFile;
	args.lineNo = lineNo;
 
	LOCK;
	(*ca_static->ca_exception_func) (args);
	UNLOCK;
}


/*
 *	CA_SIGNAL()
 */
void epicsShareAPI ca_signal(long ca_status, const char *message)
{
	ca_signal_with_file_and_lineno(ca_status, message, NULL, 0);
}


/*
 * ca_message (long ca_status)
 *
 * - if it is an unknown error code then it possible
 * that the error string generated below 
 * will be overwritten before (or while) the caller
 * of this routine is calling this routine
 * (if they call this routine again).
 */
READONLY char * epicsShareAPI ca_message (long ca_status)
{
	unsigned msgNo = CA_EXTRACT_MSG_NO(ca_status);

  	if( msgNo < NELEMENTS(ca_message_text) ){
		return ca_message_text[msgNo];
	}
	else {
		sprintf(ca_static->ca_new_err_code_msg_buf, 
	"new CA message number %u known only by server - see caerr.h", 
			msgNo);
		return ca_static->ca_new_err_code_msg_buf; 
	}
}


/*
 * ca_signal_with_file_and_lineno()
 */
void epicsShareAPI ca_signal_with_file_and_lineno(
long		ca_status, 
const char	*message, 
const char	*pfilenm, 
int		lineno)
{
  static const char  *severity[] = 
		{
		"Warning",
		"Success",
		"Error",
		"Info",
		"Fatal",
		"Fatal",
		"Fatal",
		"Fatal"
		};

  ca_printf(
"CA.Client.Diagnostic..............................................\n");

  ca_printf(
"    Message: \"%s\"\n", ca_message(ca_status));

  if(message)
    ca_printf(
"    Severity: \"%s\" Context: \"%s\"\n", 
	severity[CA_EXTRACT_SEVERITY(ca_status)],
	message);
  else
    ca_printf(
"    Severity: %s\n", severity[CA_EXTRACT_SEVERITY(ca_status)]);

  if(pfilenm){
	ca_printf(
"    Source File: %s Line Number: %d\n",
		pfilenm,
		lineno);	
  }

  /*
   *
   *
   *	Terminate execution if unsuccessful
   *
   *
   */
  if( !(ca_status & CA_M_SUCCESS) && 
		CA_EXTRACT_SEVERITY(ca_status) != CA_K_WARNING ){
      abort();
  }

  ca_printf(
"..................................................................\n");


}





/*
 *	CA_BUSY_MSG()
 *
 */
int ca_busy_message(struct ioc_in_use *piiu)
{
	caHdr hdr;
	int status;

	if(!piiu){
		return ECA_INTERNAL;
	}

  	/* 
	 * dont broadcast
	 */
    	if(piiu == piiuCast){
		return ECA_INTERNAL;
	}

	hdr = nullmsg;
	hdr.m_cmmd = htons(CA_PROTO_EVENTS_OFF);
	
	status = cac_push_msg_no_block(piiu, &hdr, NULL);
	if (status == ECA_NORMAL) {
		piiu->pushPending = TRUE;
	}
	return status;
}


/*
 * CA_READY_MSG()
 * 
 */
int ca_ready_message(struct ioc_in_use *piiu)
{
	caHdr hdr;
	int status;

	if(!piiu){
		return ECA_INTERNAL;
	}

  	/* 
	 * dont broadcast
	 */
    	if(piiu == piiuCast){
		return ECA_INTERNAL;
	}

	hdr = nullmsg;
	hdr.m_cmmd = htons(CA_PROTO_EVENTS_ON);
	
	status = cac_push_msg_no_block(piiu, &hdr, NULL);
	if (status == ECA_NORMAL) {
		piiu->pushPending = TRUE;
	}
	return status;
}


/*
 *
 * echo_request (lock must be on)
 * 
 */
int echo_request(struct ioc_in_use *piiu, ca_time *pCurrentTime)
{
	caHdr  		hdr;
	int 		status;

	hdr.m_cmmd = htons(CA_PROTO_ECHO);
	hdr.m_type = htons(0);
	hdr.m_count = htons(0);
	hdr.m_cid = htons(0);
	hdr.m_available = htons(0);
	hdr.m_postsize = 0u;

	/*
	 * If we are out of buffer space then postpone this
	 * operation until later. This avoids any possibility
	 * of a push pull deadlock (since this can be sent when 
	 * parsing the UDP input buffer).
	 */
	status = cac_push_msg_no_block(piiu, &hdr, NULL);
	if (status == ECA_NORMAL) {
		piiu->echoPending = TRUE;
		piiu->pushPending = TRUE;
		piiu->timeAtEchoRequest = *pCurrentTime;
	}

	return status;
}


/*
 *
 * NOOP_MSG (lock must be on)
 * 
 */
void noop_msg(struct ioc_in_use *piiu)
{
	caHdr  	hdr;
	int 	status;

	hdr.m_cmmd = htons(CA_PROTO_NOOP);
	hdr.m_type = htons(0);
	hdr.m_count = htons(0);
	hdr.m_cid = htons(0);
	hdr.m_available = htons(0);
	hdr.m_postsize = 0;
	
	status = cac_push_msg_no_block(piiu, &hdr, NULL);
	if (status == ECA_NORMAL) {
		piiu->pushPending = TRUE;
	}
}


/*
 * ISSUE_IOC_HOST_NAME
 * (lock must be on)
 * 
 */
void issue_client_host_name(struct ioc_in_use *piiu)
{
	unsigned	size;
	caHdr  	hdr;
	char		*pName;

	if(!piiu){
		return;
	}

  	/* 
	 * dont broadcast client identification protocol
	 */
    	if(piiu == piiuCast){
		return;
	}

	/*
	 * prior to version 4.1 client identification
	 * protocol causes a disconnect - so
	 * dont send it in this case
	 */
	if(!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
		return;
	}

	/*
	 * allocate space in the outgoing buffer
	 */
	pName =	ca_static->ca_pHostName, 
	size = strlen(pName)+1;
	hdr = nullmsg;
	hdr.m_cmmd = htons(CA_PROTO_HOST_NAME);
	hdr.m_postsize = size;
	
	cac_push_msg(piiu, &hdr, pName);

	return;
}


/*
 * ISSUE_IDENTIFY_CLIENT (lock must be on)
 * 
 */
void issue_identify_client(struct ioc_in_use *piiu)
{
	unsigned	size;
	caHdr  	hdr;
	char		*pName;

	if(!piiu){
		return;
	}

  	/* 
	 * dont broadcast client identification protocol
	 */
    	if(piiu == piiuCast){
		return;
	}

	/*
	 * prior to version 4.1 client identification
	 * protocol causes a disconnect - so
	 * dont send it in this case
	 */
	if(!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
		return;
	}

	/*
	 * allocate space in the outgoing buffer
	 */
	pName =	ca_static->ca_pUserName, 
	size = strlen(pName)+1;
	hdr = nullmsg;
	hdr.m_cmmd = htons(CA_PROTO_CLIENT_NAME);
	hdr.m_postsize = size;
	
	cac_push_msg(piiu, &hdr, pName);

	return;
}


/*
 * ISSUE_CLAIM_CHANNEL 
 */
int issue_claim_channel (chid pchan)
{
	IIU		*piiu = (IIU *) pchan->piiu;
	caHdr  		hdr;
	unsigned	size;
	const char 	*pStr;
	int		status;

	LOCK;

	if (!piiu) {
		ca_printf("CAC: nill piiu claim attempted?\n");
		UNLOCK;
		return ECA_INTERNAL;
	}

  	/* 
	 * dont broadcast
	 */
    	if (piiu == piiuCast) {
		ca_printf("CAC: UDP claim attempted?\n");
		UNLOCK;
		return ECA_INTERNAL;
	}

	if (!pchan->claimPending) {
		ca_printf("CAC: duplicate claim attempted (while disconnected)?\n");
		UNLOCK;
		return ECA_INTERNAL;
	}

	if (pchan->state==cs_conn) {
		ca_printf("CAC: duplicate claim attempted (while connected)?\n");
		UNLOCK;
		return ECA_INTERNAL;
	}

	hdr = nullmsg;
	hdr.m_cmmd = htons(CA_PROTO_CLAIM_CIU);

	if (CA_V44(CA_PROTOCOL_VERSION, piiu->minor_version_number)) {
		hdr.m_cid = pchan->cid;
		pStr = ca_name(pchan);
		size = strlen(ca_name(pchan))+1u;
	}
	else {
		hdr.m_cid = pchan->id.sid;
		pStr = NULL;
		size = 0u;
	}

	hdr.m_postsize = size;

	/*
	 * The available field is used (abused)
	 * here to communicate the minor version number
	 * starting with CA 4.1.
	 */
	hdr.m_available = htonl(CA_MINOR_VERSION);

	/*
	 * If we are out of buffer space then postpone this
	 * operation until later. This avoids any possibility
	 * of a push pull deadlock (since this is sent when 
	 * parsing the UDP input buffer).
	 */
	status = cac_push_msg_no_block(piiu, &hdr, pStr); 
	
	if (status == ECA_NORMAL) {

		/*
		 * move to the end of the list once the claim has been sent
		 */
		pchan->claimPending = FALSE;
		ellDelete (&piiu->chidlist, &pchan->node);
		ellAdd (&piiu->chidlist, &pchan->node);

		if (!CA_V42(CA_PROTOCOL_VERSION, piiu->minor_version_number)) {
			cac_reconnect_channel(pchan->cid);
		}
	}
	else {
		piiu->claimsPending = TRUE;
	}
	UNLOCK;

	return status;
}


/*
 *
 *	Default Exception Handler
 *
 *
 */
LOCAL void ca_default_exception_handler(struct exception_handler_args args)
{
	const char *pCtx;

	/*
	 * LOCK around use of sprintf buffer
	 */
	LOCK;
	if (args.chid && args.op != CA_OP_OTHER) {
		sprintf(sprintf_buf, 
			"%s - with request chan=%s op=%ld data type=%s count=%ld", 
			args.ctx,
			ca_name(args.chid),
			args.op,
			dbr_type_to_text(args.type),
			args.count);	 
		pCtx = sprintf_buf;
	}
	else {
		pCtx = args.ctx;
	}
	ca_signal_with_file_and_lineno(args.stat, pCtx, args.pFile, args.lineNo);
	UNLOCK;
}



/*
 *	CA_ADD_FD_REGISTRATION
 *
 *	call their function with their argument whenever 
 *	a new fd is added or removed
 *	(for a manager of the select system call under UNIX)
 *
 */
int epicsShareAPI ca_add_fd_registration(CAFDHANDLER *func, const void *arg)
{
	INITCHK;

	fd_register_func = func;
	fd_register_arg = arg;

  	return ECA_NORMAL;
}



/*
 *	CA_DEFUNCT
 *
 *	what is called by vacated entries in the VMS share image jump table
 *
 */
int ca_defunct()
{
	SEVCHK(ECA_DEFUNCT, NULL);
	return ECA_DEFUNCT;
}


/*
 *	CA_HOST_NAME_FUNCTION()	
 *
 * 	returns a pointer to the channel's host name
 *
 *	currently implemented as a function 
 *	(may be implemented as a MACRO in the future)
 */
READONLY char * epicsShareAPI ca_host_name_function(chid chix)
{
	IIU	*piiu;

	piiu = chix->piiu;

	if(!piiu){
		return "<local host>";
	}
	return piiu->host_name_str;
}


/*
 * ca_v42_ok(chid chan)
 */
int epicsShareAPI ca_v42_ok(chid chan)
{
	int	v42;
	IIU	*piiu;

	piiu = chan->piiu;

	v42 = CA_V42(
		CA_PROTOCOL_VERSION,
		piiu->minor_version_number);

	return v42;
}


/*
 * ca_version()
 * function that returns the CA version string
 */
READONLY char * epicsShareAPI ca_version()
{
	return CA_VERSION_STRING; 
}


/*
 *
 * 	CA_CHANNEL_STATUS
 *
 */
#ifdef vxWorks
int ca_channel_status(int tid)
{
	chid			chix;
	IIU			*piiu;
	struct CA_STATIC	*pcas;

	pcas = (struct CA_STATIC *) 
		taskVarGet(tid, (int *)&ca_static);

	if (pcas == (struct CA_STATIC *) ERROR)
		return ECA_NOCACTX;

#	define ca_static pcas
		LOCK
#	undef ca_static
	piiu = (struct ioc_in_use *) pcas->ca_iiuList.node.next;
	while(piiu){
		chix = (chid) &piiu->chidlist.node;
		while ( (chix = (chid) chix->node.next) ){
			printf(	"%s native type=%d ", 
				ca_name(chix),
				ca_field_type(chix));
			printf(	"N elements=%d IOC=%s state=", 
				ca_element_count(chix),
				piiu->host_name_str);
			switch(ca_state(chix)){
			case cs_never_conn:
				printf("never connected to an IOC");
				break;
			case cs_prev_conn:
				printf("disconnected from IOC");
				break;
			case cs_conn:
				printf("connected to an IOC");
				break;
			case cs_closed:
				printf("invalid channel");
				break;
			default:
				break;
			}
			printf("\n");
		}
		piiu = (struct ioc_in_use *)piiu->node.next;
	}
#	define ca_static pcas
		UNLOCK
#	undef ca_static
	return ECA_NORMAL;
}
#endif /*vxWorks*/


/*
 * ca_replace_printf_handler ()
 */
int epicsShareAPI ca_replace_printf_handler (
int (*ca_printf_func)(const char *pformat, va_list args)
)
{
	INITCHK;

	if (ca_printf_func) {
		ca_static->ca_printf_func = ca_printf_func;
	}
	else {
		/*
		 * os dependent
		 */
		caSetDefaultPrintfHandler();
	}

	return ECA_NORMAL;
}


/*
 *      ca_printf()
 */
int ca_printf(char *pformat, ...)
{
	int		(*ca_printf_func)(const char *pformat, va_list args);
	va_list		theArgs;
	int		status;

	va_start(theArgs, pformat);

	ca_printf_func = epicsVprintf;
	if (ca_static) {
		if (ca_static->ca_printf_func) {
			ca_printf_func = ca_static->ca_printf_func;
		}
	}

	status = (*ca_printf_func) (pformat, theArgs);

	va_end(theArgs);

	return status;
}

