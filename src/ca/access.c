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
/*									*/
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
#include	"net_convert.h"


/****************************************************************/
/*	Macros for syncing message insertion into send buffer	*/
/****************************************************************/
#define EXTMSGPTR(PIIU)\
 	((struct extmsg *) &(PIIU)->send.buf[(PIIU)->send.wtix])

/*
 * Performs worst case message alignment
 */
#define CAC_ADD_MSG(PIIU) \
{ \
	unsigned long	size; \
	struct extmsg *mp = EXTMSGPTR(PIIU); \
	size = mp->m_postsize = CA_MESSAGE_ALIGN(mp->m_postsize); \
	mp->m_postsize = htons(mp->m_postsize); \
	CAC_RING_BUFFER_WRITE_ADVANCE( \
		&(PIIU)->send, sizeof(struct extmsg) + size); \
}




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

static struct extmsg	nullmsg;

/*
 * local functions
 */

LOCAL int cac_alloc_msg(
struct ioc_in_use 	*piiu,
unsigned		extsize,
struct extmsg		**ppMsg
);
LOCAL int cac_alloc_msg_no_flush(
struct ioc_in_use 	*piiu,
unsigned		extsize,
struct extmsg		**ppMsg
);
LOCAL int	issue_get_callback(
evid 		monix, 
unsigned 	cmmd
);
LOCAL void ca_event_handler(
evid 		monix,
struct db_addr	*paddr,
int		hold,
void		*pfl
);
LOCAL void 	ca_pend_io_cleanup();
LOCAL void 	create_udp_fd();
static int issue_ca_array_put
(
unsigned			cmd,
unsigned			id,
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
);
LOCAL void ca_put_notify_action(PUTNOTIFY *ppn);
LOCAL void ca_default_exception_handler(struct exception_handler_args args);

LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
struct extmsg		*pmsg,
void			*pext
);

#ifdef CONVERSION_REQUIRED 
LOCAL void *malloc_put_convert(unsigned long size);
LOCAL void free_put_convert(void *pBuf);
#endif

LOCAL evid caIOBlockCreate(void);


/*
 *
 * 	cac_push_msg()
 *
 *	return a pointer to reserved message buffer space or
 *	nill if the message will not fit
 *
 */ 
LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
struct extmsg		*pmsg,
void			*pext
)
{
	struct extmsg	msg;
	unsigned long	bytesAvailable;
	unsigned long	actualextsize;
	unsigned long	extsize;
	unsigned long	bytesSent;

	msg = *pmsg;
	actualextsize = pmsg->m_postsize;
	extsize = CA_MESSAGE_ALIGN(pmsg->m_postsize);
	msg.m_postsize = htons(extsize);


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
	if(extsize+sizeof(msg)>piiu->send.max_msg){
		return ECA_TOLARGE;
	}
	while(TRUE){
      		struct timeval	itimeout;

		bytesAvailable = 
			cacRingBufferWriteSize(&piiu->send, FALSE);
		if(bytesAvailable>=extsize+sizeof(msg)){
			break;
		}
		/*
		 * if connection drops request
		 * cant be completed
		 */
		if(!piiu->conn_up){
			UNLOCK;
			return ECA_BADCHID;
		}
		UNLOCK;

		LD_CA_TIME (SELECT_POLL, &itimeout);
		cac_mux_io(&itimeout);

		LOCK;
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
	bytesSent = cacRingBufferWrite(
				&piiu->send, 
				pext, 
				actualextsize);
	assert(bytesSent == actualextsize);
	/*
	 * force pad bytes at the end of the message to nill
	 * if present
	 */
	{
		static 	nullBuff[32];
		int	n;

		n = extsize-actualextsize;
		if(n){
			assert(n<=sizeof(nullBuff));
			bytesSent = cacRingBufferWrite(
					&piiu->send, 
					nullBuff, 
					n);
			assert(bytesSent == n);
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
struct extmsg		**ppMsg
)
{
	unsigned 	msgsize;
	unsigned long	bytesAvailable;
	struct extmsg	*pmsg;

	msgsize = sizeof(struct extmsg)+extsize;

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

	pmsg = (struct extmsg *) &piiu->send.buf[piiu->send.wtix];
	pmsg->m_postsize = extsize;
	*ppMsg = pmsg;

	return ECA_NORMAL;
}


/*
 *
 * 	cac_alloc_msg()
 *
 *	return a pointer to reserved message buffer space or
 *	nill if the message will not fit
 *
 *	LOCK should be on
 *
 */ 
LOCAL int cac_alloc_msg(
struct ioc_in_use 	*piiu,
unsigned		extsize,
struct extmsg		**ppMsg
)
{
	unsigned 	msgsize;
	unsigned long	bytesAvailable;
	struct extmsg	*pmsg;

	msgsize = sizeof(struct extmsg)+extsize;

	/*
	 * fail if max message size exceeded
	 */
	if(msgsize>=piiu->send.max_msg){
		return ECA_TOLARGE;
	}

	bytesAvailable = cacRingBufferWriteSize(&piiu->send, TRUE);
  	while(bytesAvailable<msgsize){
	      	struct timeval	itimeout;

		UNLOCK;
		LD_CA_TIME (SELECT_POLL, &itimeout);
		cac_mux_io(&itimeout);
		LOCK;

		/*
		 * if connection drops request
		 * cant be completed
		 */
		if(!piiu->conn_up){
			return ECA_BADCHID;
		}

		bytesAvailable = cacRingBufferWriteSize(&piiu->send, TRUE);
	}

	pmsg = (struct extmsg *) &piiu->send.buf[piiu->send.wtix];
	pmsg->m_postsize = extsize;
	*ppMsg = pmsg;

	return ECA_NORMAL;
}


/*
 *
 *	CA_TASK_INITIALIZE
 *
 *
 */
int APIENTRY ca_task_initialize(void)
{
	int			status;
	struct ca_static	*ca_temp;

	if (!ca_static) {
		ca_temp = (struct ca_static *) 
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
	unsigned	sec;
	long		status;

	ca_static->ca_exception_func = ca_default_exception_handler;
	ca_static->ca_exception_arg = NULL;

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

	/* init sync group facility */
	ca_sg_init();

	/*
	 * init broadcasted search counters
	 */
	ca_static->ca_search_retry = 0;
	ca_static->ca_conn_next_retry = CA_CURRENT_TIME;
	sec = CA_RECAST_DELAY;
	ca_static->ca_conn_retry_delay.tv_sec = sec;
	ca_static->ca_conn_retry_delay.tv_usec = 
	(CA_RECAST_DELAY-sec)*USEC_PER_SEC;

	ellInit(&ca_static->ca_iiuList);
	ellInit(&ca_static->ca_ioeventlist);
	ellInit(&ca_static->ca_free_event_list);
	ellInit(&ca_static->ca_pend_read_list);
	ellInit(&ca_static->ca_pend_write_list);
	ellInit(&ca_static->putCvrtBuf);
	ellInit(&ca_static->fdInfoFreeList);
	ellInit(&ca_static->fdInfoList);

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

	return ECA_NORMAL;
}


/*
 * create_udp_fd
 */
LOCAL void create_udp_fd()
{
	int     pri;
	char	name[64];
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
	status = taskPriorityGet(VXTASKIDSELF, &pri);
	if(status<0)
		ca_signal(ECA_INTERNAL,NULL);

	strcpy(name,"RD ");
	strncat(
		name,
		taskName(VXTHISTASKID),
		sizeof(name)-strlen(name)-1);

	status = taskSpawn(
				name,
                           	pri-1,
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
	if (status<0) {
		ca_signal(ECA_INTERNAL,NULL);
	}

	ca_static->recv_tid = status;

#endif
}


/*
 * CA_MODIFY_HOST_NAME()
 *
 * Modify or override the default 
 * client host name.
 */
int APIENTRY ca_modify_host_name(char *pHostName)
{
	char		*pTmp;
	unsigned	size;
	IIU		*piiu;

	INITCHK;

	size = strlen(pHostName)+1;
	if (size > STRING_LIMIT){
		return ECA_STRTOBIG;
	}

	pTmp = malloc(size);
	if(!pTmp){
		return ECA_ALLOCMEM;
	}

	if(ca_static->ca_pHostName){
		free(ca_static->ca_pHostName);
	}
	ca_static->ca_pHostName = pTmp;

	/*
	 * force null termination
	 */
	strncpy(	
		ca_static->ca_pHostName, 
		pHostName, 
		size-1);
	ca_static->ca_pHostName[size-1]='\0';

	/*
	 * update all servers we are currently
	 * attached to.
	 */
	LOCK;
	piiu = (struct ioc_in_use *) ca_static->ca_iiuList.node.next;
	while(piiu){
		issue_client_host_name(piiu);
		piiu = (struct ioc_in_use *) piiu->node.next;
	}
	UNLOCK;

	return ECA_NORMAL;
}



/*
 * CA_MODIFY_USER_NAME()
 *
 * Modify or override the default 
 * client user name.
 */
int APIENTRY ca_modify_user_name(char *pClientName)
{
	char		*pTmp;
	unsigned	size;
	IIU		*piiu;

	INITCHK;

	size = strlen(pClientName)+1;
	if (size > STRING_LIMIT){
		return ECA_STRTOBIG;
	}

	pTmp = malloc(size);
	if(!pTmp){
		return ECA_ALLOCMEM;
	}

	if(ca_static->ca_pUserName){
		free(ca_static->ca_pUserName);
	}
	ca_static->ca_pUserName = pTmp;

	/*
	 * force null termination
	 */
	strncpy(	
		ca_static->ca_pUserName, 
		pClientName, 
		size-1);
	ca_static->ca_pUserName[size-1]='\0';

	/*
	 * update all servers we are currently
	 * attached to.
	 */
	LOCK;
	piiu = (struct ioc_in_use *) ca_static->ca_iiuList.node.next;
	while(piiu){
		issue_identify_client(piiu);
		piiu = (struct ioc_in_use *) piiu->node.next;
	}
	UNLOCK;

	return ECA_NORMAL;
}



/*
 *	CA_TASK_EXIT()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
int APIENTRY ca_task_exit (void)
{
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
	evid            	monix;
	evid            	monixNext;
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
				ca_static->ca_fd_register_arg,
				piiu->sock_chan,
				FALSE);
		}
		if (socket_close(piiu->sock_chan) < 0){
			ca_signal(
				ECA_INTERNAL, 
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
	monix = (evid) ellFirst(&ca_static->ca_pend_read_list);
	while (monix) {
		monixNext = (evid) ellNext (&monix->node);
		caIOBlockFree (monix);
		monix = monixNext;
	}

	/* remove any pending write blocks */
	monix = (evid) ellFirst(&ca_static->ca_pend_write_list);
	while (monix) {
		monixNext = (evid) ellNext (&monix->node);
		caIOBlockFree (monix);
		monix = monixNext;
	}

	/* remove any pending io event blocks */
	ellFree(&ca_static->ca_ioeventlist);

	/* remove put convert block free list */
	ellFree(&ca_static->putCvrtBuf);

	/* reclaim sync group resources */
	ca_sg_shutdown(ca_static);

	/* remove remote waiting ev blocks */
	ellFree(&ca_static->ca_free_event_list);

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
	status = bucketFree(ca_static->ca_pSlowBucket);
	assert(status == BUCKET_SUCCESS);
	status = bucketFree(ca_static->ca_pFastBucket);
	assert(status == BUCKET_SUCCESS);

	/*
	 * free beacon hash table
	 */
	freeBeaconHash(ca_static);

	UNLOCK;
}




/*
 *
 *	CA_BUILD_AND_CONNECT
 *
 * 	backwards compatible entry point to ca_search_and_connect()
 */
int APIENTRY ca_build_and_connect
(
 char *name_str,
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
 *
 *	ca_search_and_connect()	
 *
 *
 */
int APIENTRY ca_search_and_connect
(
 char *name_str,
 chid *chixptr,
 void (*conn_func) (struct connection_handler_args),
 void *puser
)
{
	long            status;
	chid            chix;
	int             strcnt;
	int 		sec;

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
		struct db_addr  tmp_paddr;

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
			size = sizeof(*chix) + strcnt + sizeof(struct db_addr);
			*chixptr = chix = (chid) calloc(1,size);
			if (!chix){
				return ECA_ALLOCMEM;
			}
			chix->id.paddr = (struct db_addr *) 
				(strcnt + (char *) (chix + 1));
			*chix->id.paddr = tmp_paddr;
			chix->puser = puser;
			chix->pConnFunc = conn_func;
			chix->type = chix->id.paddr->field_type;
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
			UNLOCK;

			LOCKEVENTS;
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
			UNLOCKEVENTS;
			return ECA_NORMAL;
		}
	}
#endif

	if (!ca_static->ca_piiuCast) {
		create_udp_fd();
		if(!ca_static->ca_piiuCast){
			return ECA_NOCAST;
		}
	}

	/* allocate CHIU (channel in use) block */
	/* also allocate enough for the channel name */
	*chixptr = chix = (chid) calloc(1, sizeof(*chix) + strcnt);
	if (!chix){
		return ECA_ALLOCMEM;
	}

	LOCK;
	chix->cid = CLIENT_SLOW_ID_ALLOC;
	status = bucketAddItemUnsignedId(pSlowBucket, &chix->cid, chix);
	if(status != BUCKET_SUCCESS){
		*chixptr = (chid) NULL;
		free((char *) chix);
		UNLOCK;
		return ECA_ALLOCMEM;
	}

	chix->puser = puser;
	chix->pConnFunc = conn_func;
	chix->type = TYPENOTCONN; /* invalid initial type 	 */
	chix->count = 0; 	/* invalid initial count	 */
	chix->id.sid = ~0L;	/* invalid initial server id 	 */

	chix->state = cs_never_conn;
	ellInit(&chix->eventq);

	/* Save this channels name for retry if required */
	strncpy((char *)(chix + 1), name_str, strcnt);

	chix->piiu = piiuCast;
	ellAdd(&piiuCast->chidlist, &chix->node);

	/*
	 * reset broadcasted search counters
	 */
	ca_static->ca_search_retry = 0;
	ca_static->ca_conn_next_retry = CA_CURRENT_TIME;
	sec = CA_RECAST_DELAY;
	ca_static->ca_conn_retry_delay.tv_sec = sec;
	ca_static->ca_conn_retry_delay.tv_usec = 
		(CA_RECAST_DELAY-sec)*USEC_PER_SEC;

	UNLOCK;

	/*
	 * Connection Management takes care 
	 * of sending the the search requests
	 */

	if (!chix->pConnFunc) {
		SETPENDRECV;
	}

	return ECA_NORMAL;

}



/*
 * SEARCH_MSG()
 * 
 * NOTE:	*** lock must be applied while in this routine ***
 * 
 */
int search_msg(
chid            chix,
int             reply_type
)
{
	int			status;
	int    			size;
	int    			cmd;
	struct extmsg		*mptr;
	struct ioc_in_use	*piiu;

	piiu = chix->piiu;

	size = strlen((char *)(chix+1))+1;
	cmd = IOC_SEARCH;

	LOCK;
	status = cac_alloc_msg_no_flush (piiu, size, &mptr);
	if(status != ECA_NORMAL){
		UNLOCK;
		return status;
	}

	mptr->m_cmmd = htons (cmd);
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

	CAC_ADD_MSG(piiu);

	/*
	 * increment the number of times we have tried this
	 * channel
	 */
	if (chix->retry<MAXCONNTRIES) {
		chix->retry++;
	}

	UNLOCK;

	piiu->send_needed = TRUE;

	return ECA_NORMAL;
}



/*
 * CA_ARRAY_GET()
 * 
 * 
 */
int APIENTRY ca_array_get
(
chtype 		type,
unsigned long 	count,
chid 		chix,
void 		*pvalue
)
{
	int		status;
	evid		monix;

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

	monix = caIOBlockCreate();
	if (monix) {

		monix->chan = chix;
		monix->type = type;
		monix->count = count;
		monix->usr_arg = pvalue;

		LOCK;
		ellAdd(&pend_read_list, &monix->node);
		UNLOCK;

		status = issue_get_callback(monix, IOC_READ);
		if (status == ECA_NORMAL) {
			SETPENDRECV;
		}
	} 
	else {
		status = ECA_ALLOCMEM;
	}

	return status;
}



/*
 * CA_ARRAY_GET_CALLBACK()
 */
int APIENTRY ca_array_get_callback
(
chtype type,
unsigned long count,
chid chix,
void (*pfunc) (struct event_handler_args),
void *arg
)
{
	int	status;
	evid	monix;

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

	monix = caIOBlockCreate();

	if (monix) {

		monix->chan = chix;
		monix->usr_func = pfunc;
		monix->usr_arg = arg;
		monix->type = type;
		monix->count = count;

		LOCK;
		ellAdd(&pend_read_list, &monix->node);
		UNLOCK;

		status = issue_get_callback (monix, IOC_READ_NOTIFY);
	} 
	else {
		status = ECA_ALLOCMEM;
	}

	return status;
}


/*
 * caIOBlockCreate()
 */
LOCAL evid caIOBlockCreate(void)
{
	int	status;
	evid	pIOBlock;

	LOCK;

	pIOBlock = (evid) ellGet (&free_event_list);
	if (pIOBlock) {
		memset ((char *)pIOBlock, 0, sizeof(*pIOBlock));
	}
	else {
		pIOBlock = (evid) calloc(1, sizeof(*pIOBlock));
	}

	if (pIOBlock) {
		pIOBlock->id = CLIENT_FAST_ID_ALLOC;
		status = bucketAddItemUnsignedId(
				pFastBucket, 
				&pIOBlock->id, 
				pIOBlock);
		if(status != BUCKET_SUCCESS){
			free(pIOBlock);
			pIOBlock = NULL;
		}
	}

	UNLOCK;

	return pIOBlock;
}


/*
 * caIOBlockFree()
 */
void caIOBlockFree(evid pIOBlock)
{
	int	status;

	LOCK;
	status = bucketRemoveItemUnsignedId(
			ca_static->ca_pFastBucket, 
			&pIOBlock->id);
	assert (status == BUCKET_SUCCESS);
	pIOBlock->id = ~0U; /* this id always invalid */
	ellAdd (&free_event_list, &pIOBlock->node);
	UNLOCK;
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
	struct extmsg		hdr;
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

	piiu->send_needed = TRUE;

	return status;
}


/*
 *	CA_ARRAY_PUT_CALLBACK()
 *
 */
int APIENTRY ca_array_put_callback
(
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue,
void				(*pfunc)(struct event_handler_args),
void				*usrarg
)
{
	IIU	*piiu;
	int	status;
	evid 	monix;

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

	if(!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
		return ECA_NOSUPPORT;
	}

	if(piiu){
		monix = (evid) caIOBlockCreate();
		if (!monix){
			return ECA_ALLOCMEM;
		}

		monix->chan = chix;
		monix->usr_func = pfunc;
		monix->usr_arg = usrarg;
		monix->type = type;
		monix->count = count;
	}
#ifdef vxWorks
	else{
		CACLIENTPUTNOTIFY	*ppn;
		int 			size;

		size = dbr_size_n(type,count);
		LOCK;

		ppn = chix->ppn;
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
				ppn = chix->ppn = NULL;
			}
		}

		if(!ppn){
			ppn = (CACLIENTPUTNOTIFY *) 
				calloc(1, sizeof(*ppn)+size);
			if(!ppn){
				UNLOCK;
				return ECA_ALLOCMEM;
			}
			chix->ppn = ppn;
			ppn->pcas = ca_static;
			ppn->dbPutNotify.userCallback = 
				ca_put_notify_action;
			ppn->dbPutNotify.usrPvt = chix;
			ppn->dbPutNotify.paddr = chix->id.paddr;
			ppn->dbPutNotify.pbuffer = (ppn+1);
		}
		ppn->busy = TRUE;
		ppn->caUserCallback = pfunc;
		ppn->caUserArg = usrarg;
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
		if(status){
			if(status==S_db_Blocked){
				return ECA_PUTCBINPROG;
			}
			ppn->busy = FALSE;
			return ECA_PUTFAIL;
		}
		return ECA_NORMAL;
	}
#endif /*vxWorks*/

	status = issue_ca_array_put(
			IOC_WRITE_NOTIFY, 
			monix->id,
			type, 
			count, 
			chix, 
			pvalue);
	if(status != ECA_NORMAL){
		if(chix->piiu){
			caIOBlockFree(monix);
		}
		return status;
	}


	if(piiu){
		LOCK;
		ellAdd(&pend_write_list, &monix->node);
		UNLOCK;
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
	struct ca_static	*pcas;
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
int APIENTRY ca_array_put (
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
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

	return issue_ca_array_put(IOC_WRITE, ~0U, type, count, chix, pvalue);
}


/*
 * issue_ca_array_put()
 */
LOCAL int issue_ca_array_put
(
unsigned			cmd,
unsigned			id,
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
)
{ 
	int			status;
	struct ioc_in_use	*piiu;
	struct extmsg		hdr;
  	int  			postcnt;
  	unsigned		size_of_one;
  	int 			i;
#	ifdef CONVERSION_REQUIRED 
	void			*pCvrtBuf;
  	void			*pdest;
#	endif /*CONVERSION_REQUIRED*/

	piiu = chix->piiu;
	size_of_one = dbr_size[type];
	postcnt = dbr_size_n(type,count);

	/*
	 *
 	 * Each of their strings is checked for prpoper size
	 * so there is no chance that they could crash the 
	 * CA server.
	 */
	if(type == DBR_STRING){
		char *pstr = (char *)pvalue;

		if(count == 1)
			postcnt = strlen(pstr)+1;

    		for(i=0; i< count; i++){
			unsigned int strsize;

          		strsize = strlen(pstr) + 1;

          		if(strsize>size_of_one)
            			return ECA_STRTOBIG;

			pstr += size_of_one;
		}
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
#		if DBR_INT != DBR_SHORT
      		case	DBR_INT:
#		endif /*DBR_INT != DBR_SHORT*/
        		*(short *)pdest = htons(*(short *)pvalue);
        		break;

      		case	DBR_FLOAT:
        		htonf(pvalue, pdest);
        		break;

      		case	DBR_DOUBLE: 
        		htond(pvalue, pdest);
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
    	hdr.m_count 		= htons(count);
    	hdr.m_cid 		= chix->id.sid;
    	hdr.m_available 	= id;
	hdr.m_postsize 		= postcnt;

	status = cac_push_msg(piiu, &hdr, pvalue);

#	ifdef CONVERSION_REQUIRED
	free_put_convert(pCvrtBuf);
#	endif /*CONVERSION_REQUIRED*/

	piiu->send_needed = TRUE;

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
int APIENTRY ca_change_connection_event
(
chid		chix,
void 		(*pfunc)(struct connection_handler_args)
)
{

  	INITCHK;
  	LOOSECHIXCHK(chix);

	if(chix->pConnFunc == pfunc)
  		return ECA_NORMAL;

  	LOCK;
  	if(chix->type == TYPENOTCONN){
		if(!chix->pConnFunc && chix->state==cs_never_conn){
    			CLRPENDRECV(FALSE);
		}
		if(!pfunc){
    			SETPENDRECV;
		}
	}
  	chix->pConnFunc = pfunc;
  	UNLOCK;

  	return ECA_NORMAL;
}


/*
 * ca_replace_access_rights_event
 */
int APIENTRY ca_replace_access_rights_event(
chid 	chan, 
void    (*pfunc)(struct access_rights_handler_args))
{
	struct access_rights_handler_args 	args;

  	INITCHK;
  	LOOSECHIXCHK(chan);

    	chan->pAccessRightsFunc = pfunc;

	/*
	 * make certain that it runs at least once
	 */
	if(chan->state == cs_conn){
		args.chid = chan;
		args.ar = chan->ar;
		(*chan->pAccessRightsFunc)(args);
	}

  	return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for asynch exceptions
 *
 */
int APIENTRY ca_add_exception_event
(
void 		(*pfunc)(struct exception_handler_args),
void 		*arg
)
{

  INITCHK;
  LOCK;
  if(pfunc){
    ca_static->ca_exception_func = pfunc;
    ca_static->ca_exception_arg = arg;
  }
  else{
    ca_static->ca_exception_func = ca_default_exception_handler;
    ca_static->ca_exception_arg = NULL;
  }
  UNLOCK;

  return ECA_NORMAL;
}


/*
 * Specify an event subroutine to be run once pending IO completes
 * 
 * 1) event sub runs right away if no IO pending 
 * 2) otherwise event sub runs when pending IO completes
 * 
 * Undocumented entry for the VAX OPI which may vanish in the future.
 *
 */
int APIENTRY ca_add_io_event
(
void 		(*ast)(),
void 		*astarg
)
{
  register struct pending_io_event	*pioe;

  INITCHK;

  if(pndrecvcnt<1)
    (*ast)(astarg);
  else{
    pioe = (struct pending_io_event *) malloc(sizeof(*pioe));
    if(!pioe)
      return ECA_ALLOCMEM;
    memset((char *)pioe,0,sizeof(*pioe));
    pioe->io_done_arg = astarg;
    pioe->io_done_sub = ast;
    LOCK;
    ellAdd(&ioeventlist, &pioe->node);
    UNLOCK;
  }

  return ECA_NORMAL;
}




/*
 *	CA_ADD_MASKED_ARRAY_EVENT
 *
 *
 */
int APIENTRY ca_add_masked_array_event
(
chtype 		type,
unsigned long	count,
chid 		chix,
void 		(*ast)(struct event_handler_args),
void 		*astarg,
ca_real		p_delta,
ca_real		n_delta,
ca_real		timeout,
evid 		*monixptr,
long		mask
)
{
  	evid		monix;
  	int		status;
  	int		size;

  	INITCHK;
  	LOOSECHIXCHK(chix);

  	if(INVALID_DB_REQ(type))
    		return ECA_BADTYPE;

	/*
	 * Check for huge waveform
	 */
	if(dbr_size_n(type,count)>MAX_MSG_SIZE-sizeof(caHdr)){
		return ECA_TOLARGE;
	}

  	if(count > chix->count && chix->type != TYPENOTCONN)
    		return ECA_BADCOUNT;

  	if(!mask)
    		return ECA_BADMASK;

  	LOCK;

  	/*	event descriptor	*/
# 	ifdef vxWorks
  	{
    		static int			dbevsize;

    		if(!chix->piiu){

      			if(!dbevsize){
        			dbevsize = db_sizeof_event_block();
			}
      			size = sizeof(*monix)+dbevsize;
      			if(!(monix = (evid)ellGet(&dbfree_ev_list))){
        			monix = (evid)malloc(size);
      			}
    		}
    		else{
			monix = caIOBlockCreate();
    		}
  	}
# 	else
		monix = caIOBlockCreate();
# 	endif

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
  	monix->usr_arg	= astarg;
  	monix->type	= type;
  	monix->count	= count;
  	monix->p_delta	= p_delta;
  	monix->n_delta	= n_delta;
  	monix->timeout	= timeout;
  	monix->mask	= mask;

# 	ifdef vxWorks
  	{
    		if(!chix->piiu){
      			status = db_add_event(	
					evuser,
					chix->id.paddr,
					ca_event_handler,
					monix,
					mask,
					monix+1);
      			if(status == ERROR){
				UNLOCK;
       				return ECA_DBLCLFAIL; 
      			}

      			/* 
			 * Place in the channel list 
			 * - do it after db_add_event so there
			 * is no chance that it will be deleted 
			 * at exit before it is completely created
      			 */
      			ellAdd(&chix->eventq, &monix->node);

      			/* 
			 * force event to be called at least once
      			 * return warning msg if they have made the queue to full 
		 	 * to force the first (untriggered) event.
       			 */
      			if(db_post_single_event(monix+1)==ERROR){
				UNLOCK;
       		 		return ECA_OVEVFAIL;
      			}

      			UNLOCK;
      			return ECA_NORMAL;
    		} 
  	}
# 	endif

  	/* It can be added to the list any place if it is remote */
  	/* Place in the channel list */
  	ellAdd(&chix->eventq, &monix->node);

  	UNLOCK;

	if(chix->state == cs_conn){
  		return ca_request_event(monix);
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
	msg.m_header.m_cmmd = htons(IOC_EVENT_ADD);
	msg.m_header.m_available = monix->id;
	msg.m_header.m_type = htons(monix->type);
	msg.m_header.m_count = htons(count);
	msg.m_header.m_cid = chix->id.sid;
	msg.m_header.m_postsize = sizeof(msg.m_info);

	/* msg body	 */
	htonf(&monix->p_delta, &msg.m_info.m_hval);
	htonf(&monix->n_delta, &msg.m_info.m_lval);
	htonf(&monix->timeout, &msg.m_info.m_toval);
	msg.m_info.m_mask = htons(monix->mask);

	status = cac_push_msg(piiu, &msg.m_header, &msg.m_info);

	piiu->send_needed = TRUE;

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
evid 		monix,
struct db_addr	*paddr,
int		hold,
void		*pfl
)
{
  	register int 		status;
  	register int		count;
  	register chtype		type = monix->type;
  	union db_access_val	valbuf;
  	void			*pval;
  	register unsigned	size;
	struct tmp_buff{
		ELLNODE		node;
		unsigned	size;
	};
	struct tmp_buff		*pbuf = NULL;

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

  	if(type == paddr->field_type){
    		pval = paddr->pfield;
    		status = OK;
  	}
  	else{
		size = dbr_size_n(type,count);

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
			if(pbuf->size >= size){
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
				type,
				pval,
				count,
				pfl);

  	/*
	 * Call user's callback
   	 */
	LOCKEVENTS;
	{
		struct event_handler_args args;
		
		args.usr = monix->usr_arg;
		args.chid = monix->chan;
		args.type = type;
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
	UNLOCKEVENTS;

	/*
	 *
	 * insert the buffer back into the que in size order if
	 * one was used.
	 */
	if(pbuf){
		struct tmp_buff		*ptbuf;
		LOCK;
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
		UNLOCK;
	}

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
int APIENTRY ca_clear_event (evid monix)
{
	int		status;
	chid   		chix = monix->chan;
	struct extmsg 	hdr;
	evid		lkup;

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
					&monix->id);
		UNLOCK;
		if (lkup != monix) {
			return ECA_BADMONID;
		}
	}

	/* disable any further events from this event block */
	monix->usr_func = NULL;

#ifdef vxWorks
	if (!chix->piiu) {
		register int    status;

		/*
		 * dont allow two threads to delete the same moniitor at once
		 */
		LOCK;
		ellDelete(&chix->eventq, &monix->node);
		status = db_cancel_event(monix + 1);
		ellAdd(&dbfree_ev_list, &monix->node);
		UNLOCK;

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
		hdr.m_cmmd = htons(IOC_EVENT_CANCEL);
		hdr.m_available = monix->id;
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
		ellDelete(&monix->chan->eventq, &monix->node);
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
int APIENTRY ca_clear_channel (chid chix)
{
	int				status;
	evid   				monix;
	struct ioc_in_use 		*piiu = chix->piiu;
	struct extmsg 			hdr;
	enum channel_state		old_chan_state;

	LOOSECHIXCHK(chix);

	/* disable their further use of deallocated channel */
	chix->type = TYPENOTINUSE;
	old_chan_state = chix->state;
	chix->state = cs_closed;

	/* the while is only so I can break to the lock exit */
	LOCK;
	/* disable any further events from this channel */
	for (monix = (evid) chix->eventq.node.next;
	     monix;
	     monix = (evid) monix->node.next)
		monix->usr_func = NULL;
	/* disable any further get callbacks from this channel */
	for (monix = (evid) pend_read_list.node.next;
	     monix;
	     monix = (evid) monix->node.next)
		if (monix->chan == chix)
			monix->usr_func = NULL;
	/* disable any further put callbacks from this channel */
	for (monix = (evid) pend_write_list.node.next;
	     monix;
	     monix = (evid) monix->node.next)
		if (monix->chan == chix)
			monix->usr_func = NULL;

#ifdef vxWorks
	if (!chix->piiu) {
		CACLIENTPUTNOTIFY	*ppn;
		int             	status;

		/*
		 * clear out the events for this channel
		 */
		while (monix = (evid) ellGet(&chix->eventq)) {
			status = db_cancel_event(monix + 1);
			assert (status == OK);
			ellAdd(&dbfree_ev_list, &monix->node);
		}

		/*
		 * cancel any outstanding put notifies
		 */
		if(chix->ppn){
			ppn = chix->ppn;
			if(ppn->busy){
				dbNotifyCancel(&ppn->dbPutNotify);
			}
			free(ppn);
		}

		/*
		 * clear out this channel
		 */
		ellDelete(&local_chidlist, &chix->node);
		free((char *) chix);

		UNLOCK;
		return ECA_NORMAL;
	}
#endif

	/*
	 * dont send the message if not conn 
	 * (just delete from the queue and return)
	 * 
	 * check for conn state while locked to avoid a race
	 */
	if(old_chan_state != cs_conn){
		UNLOCK;
		clearChannelResources (chix->cid);
		return ECA_NORMAL;
	}

	/*
	 * clear events and all other resources for this chid on the
	 * IOC
	 */

	/* msg header	 */
	hdr.m_cmmd = htons(IOC_CLEAR_CHANNEL);
	hdr.m_available = chix->cid;
	hdr.m_type = htons(0);
	hdr.m_count = htons(0);
	hdr.m_cid = chix->id.sid;
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
 * clearChannelResources()
 */
void clearChannelResources(unsigned id)
{
	struct ioc_in_use       *piiu;
	chid			chix;
	evid			monix;
	evid			next;
	int			status;

	LOCK;

	chix = bucketLookupItemUnsignedId(ca_static->ca_pSlowBucket, &id);
	assert ( chix!=NULL );

	piiu = chix->piiu;

	/*
	 * remove any orphaned get callbacks for this
	 * channel
	 */
	for (monix = (evid) ellFirst (&ca_static->ca_pend_read_list);
	     monix;
	     monix = next) {
		next = (evid) ellNext (&monix->node);
		if (monix->chan == chix) {
			ellDelete (
				&ca_static->ca_pend_read_list,
				&monix->node);
			caIOBlockFree (monix);
		}
	}
	for (monix = (evid) ellFirst (&chix->eventq);
	     monix;
	     monix = next){
		assert (monix->chan == chix);
		next = (evid) ellNext (&monix->node);
		caIOBlockFree(monix);
	}
	ellDelete (&piiu->chidlist, &chix->node);
	status = bucketRemoveItemUnsignedId (
			ca_static->ca_pSlowBucket, &chix->cid);
	assert (status == BUCKET_SUCCESS);
	free (chix);
	if (piiu!=piiuCast && !piiu->chidlist.count){
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
/*	ca_flush_io() is called by this routine.			*/
/************************************************************************/
int APIENTRY ca_pend(ca_real timeout, int early)
{
	struct timeval	beg_time;
	struct timeval	cur_time;
	ca_real		delay;

  	INITCHK;

	if(timeout<0.0){
		return ECA_TIMEOUT;
	}

	if(EVENTLOCKTEST){
    		return ECA_EVDISALLOW;
	}

  	/*	
	 * Flush the send buffers
	 *
	 * Also takes care of outstanding recvs
	 * for single threaded clients 
	 */
	ca_flush_io();

    	if(pndrecvcnt<1 && early){
        	return ECA_NORMAL;
	}

	cac_gettimeval(&beg_time);

  	while(TRUE){
		ca_real 	remaining;
		struct timeval	tmo;

    		if(pndrecvcnt<1 && early)
        		return ECA_NORMAL;

    		if(timeout == 0.0){
			remaining = SELECT_POLL;
		}
		else{

			cac_gettimeval (&cur_time);
			delay = cac_time_diff (&cur_time, &beg_time);
			remaining = timeout-delay;
      			if(remaining<=0.0){
				if(early){
					ca_pend_io_cleanup();
				}
				ca_flush_io();
        			return ECA_TIMEOUT;
      			}
			/*
			 * Allow for CA background labor
			 */
			remaining = min(SELECT_POLL, remaining);
  		}    

		tmo.tv_sec = remaining;
		tmo.tv_usec = (remaining-tmo.tv_sec)*USEC_PER_SEC;
		cac_block_for_io_completion(&tmo);
	}
}


/*
 * cac_time_diff()
 */
ca_real cac_time_diff (ca_time *pTVA, ca_time *pTVB)
{
	ca_real 	delay;

	delay = pTVA->tv_sec - pTVB->tv_sec;
	if(pTVA->tv_usec>pTVB->tv_usec){
		delay += (pTVA->tv_usec - pTVB->tv_usec) /
				(ca_real)(USEC_PER_SEC);
	}
	else{
		delay -= 1.0;
		delay += (USEC_PER_SEC - pTVB->tv_usec + pTVA->tv_usec) /
				(ca_real)(USEC_PER_SEC);
	}

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
 *
 * set pending IO count back to zero and
 * send a sync to each IOC and back. dont
 * count reads until we recv the sync
 *
 */
LOCAL	void ca_pend_io_cleanup()
{
	struct ioc_in_use       *piiu;

	LOCK;
	for(	piiu = (IIU *) iiuList.node.next;
		piiu;
		piiu = (IIU *) piiu->node.next){

		struct extmsg hdr;

		if(piiu == piiuCast || !piiu->conn_up){
			continue;
		}

		piiu->cur_read_seq++;

		hdr = nullmsg;
		hdr.m_cmmd = htons(IOC_READ_SYNC);
		cac_push_msg(piiu, &hdr, NULL);
	}
	UNLOCK;
	pndrecvcnt = 0;
}




/*
 *	CA_FLUSH_IO()
 *
 * 	Flush the send buffer 
 *
 */
int APIENTRY ca_flush_io()
{
	struct ioc_in_use	*piiu;
	struct timeval  	timeout;
	int			pending;
	unsigned long		bytesPending;

  	INITCHK;

	pending = TRUE;
      	timeout.tv_usec = 0;	
	timeout.tv_sec = 0;
	while(pending){

		/*
		 * perform socket io
		 * and process recv backlog
		 */
		cac_mux_io(&timeout);

		/*
		 * wait for all buffers to flush
		 */
		pending = FALSE;
		LOCK;
		for(	piiu = (IIU *) iiuList.node.next;
			piiu;
			piiu = (IIU *) piiu->node.next){

			if(piiu == piiuCast || piiu->conn_up == FALSE){

				continue;
			}

			bytesPending = cacRingBufferReadSize(
						&piiu->send, 
						FALSE);
			if(bytesPending != 0){
				pending = TRUE;
			}
		}
		UNLOCK;

		LD_CA_TIME (SELECT_POLL, &timeout);
	}

  	return ECA_NORMAL;
}


/*
 *	CA_TEST_IO ()
 *
 */
int APIENTRY ca_test_io()
{
    	if(pndrecvcnt<1){
        	return ECA_IODONE;
	}
	else{
		return ECA_IOINPROGRESS;
	}
}


/*
 *	CA_SIGNAL()
 *
 *
 */
void APIENTRY ca_signal(long ca_status,char *message)
{
	ca_signal_with_file_and_lineno(ca_status, message, NULL, 0);
}


/*
 * ca_signal_with_file_and_lineno()
 */
void APIENTRY ca_signal_with_file_and_lineno(
long		ca_status, 
char		*message, 
char		*pfilenm, 
int		lineno)
{
  static  char  *severity[] = 
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

  if( CA_EXTRACT_MSG_NO(ca_status) >= NELEMENTS(ca_message_text) ){
    message = "corrupt status";
    ca_status = ECA_INTERNAL;
  }

  ca_printf(
"CA.Client.Diagnostic..............................................\n");

  ca_printf(
"    Message: \"%s\"\n", ca_message_text[CA_EXTRACT_MSG_NO(ca_status)]);

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
void ca_busy_message(struct ioc_in_use *piiu)
{
	struct extmsg  	hdr;

	if(!piiu){
		return;
	}

  	/* 
	 * dont broadcast
	 */
    	if(piiu == piiuCast)
		return;

	hdr = nullmsg;
	hdr.m_cmmd = htons(IOC_EVENTS_OFF);
	
	cac_push_msg(piiu, &hdr, NULL);

	piiu->send_needed = TRUE;
}


/*
 * CA_READY_MSG()
 * 
 */
void ca_ready_message(struct ioc_in_use *piiu)
{
	struct extmsg  	hdr;

	if(!piiu){
		return;
	}

  	/* 
	 * dont broadcast
	 */
    	if(piiu == piiuCast){
		return;
	}

	hdr = nullmsg;
	hdr.m_cmmd = htons(IOC_EVENTS_ON);
	
	cac_push_msg(piiu, &hdr, NULL);

	piiu->send_needed = TRUE;
}


/*
 *
 * echo_request (lock must be on)
 * 
 */
int echo_request(struct ioc_in_use *piiu, ca_time *pCurrentTime)
{
	int		status;
	struct extmsg  	*phdr;

	status = cac_alloc_msg_no_flush (piiu, sizeof(*phdr), &phdr);
	if (status) {
		return status;
	}

	phdr->m_cmmd = htons(IOC_ECHO);
	phdr->m_type = htons(0);
	phdr->m_count = htons(0);
	phdr->m_cid = htons(0);
	phdr->m_available = htons(0);
	phdr->m_postsize = 0;

	CAC_ADD_MSG(piiu);
	
	piiu->echoPending = TRUE;
	piiu->send_needed = TRUE;
	piiu->timeAtEchoRequest = *pCurrentTime;

	return ECA_NORMAL;
}


/*
 *
 * NOOP_MSG (lock must be on)
 * 
 */
void noop_msg(struct ioc_in_use *piiu)
{
	struct extmsg  	hdr;

	hdr.m_cmmd = htons(IOC_NOOP);
	hdr.m_type = htons(0);
	hdr.m_count = htons(0);
	hdr.m_cid = htons(0);
	hdr.m_available = htons(0);
	hdr.m_postsize = 0;
	
	cac_push_msg(piiu, &hdr, NULL);

	piiu->send_needed = TRUE;
}


/*
 * ISSUE_IOC_HOST_NAME
 * (lock must be on)
 * 
 */
void issue_client_host_name(struct ioc_in_use *piiu)
{
	unsigned	size;
	struct extmsg  	hdr;
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
	hdr.m_cmmd = htons(IOC_HOST_NAME);
	hdr.m_postsize = size;
	
	cac_push_msg(piiu, &hdr, pName);

	piiu->send_needed = TRUE;

	return;
}


/*
 * ISSUE_IDENTIFY_CLIENT (lock must be on)
 * 
 */
void issue_identify_client(struct ioc_in_use *piiu)
{
	unsigned	size;
	struct extmsg  	hdr;
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
	hdr.m_cmmd = htons(IOC_CLIENT_NAME);
	hdr.m_postsize = size;
	
	cac_push_msg(piiu, &hdr, pName);

	piiu->send_needed = TRUE;

	return;
}


/*
 * ISSUE_CLAIM_CHANNEL (lock must be on)
 * 
 */
void issue_claim_channel(struct ioc_in_use *piiu, chid pchan)
{
	struct extmsg  	hdr;
	unsigned	size;
	char		*pName;

	if(!piiu){
		return;
	}

  	/* 
	 * dont broadcast
	 */
    	if(piiu == piiuCast)
		return;

	hdr = nullmsg;
	hdr.m_cmmd = htons(IOC_CLAIM_CIU);

	if(CA_V44(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
		hdr.m_cid = pchan->cid;
		pName = ca_name(pchan);
		size = strlen(pName)+1;
	}
	else {
		hdr.m_cid = pchan->id.sid;
		pName = NULL;
		size = 0;
	}

	hdr.m_postsize = size;

	/*
	 * The available field is used (abused)
	 * here to communicate the miner version number
	 * starting with CA 4.1.
	 */
	hdr.m_available = htonl(CA_MINOR_VERSION);

	cac_push_msg(piiu, &hdr, pName);

	piiu->send_needed = TRUE;
}



/*
 *
 *	Default Exception Handler
 *
 *
 */
LOCAL void ca_default_exception_handler(struct exception_handler_args args)
{
	char *pName;

	if(args.chid){
		pName = ca_name(args.chid);
	}
	else{
		pName = "?";
	}

	/*
	 * LOCK around use of sprintf buffer
	 */
	LOCK;
	sprintf(sprintf_buf, 
		"%s - with request chan=%s op=%d data type=%s count=%d\n", 
		args.ctx,
		pName,
		args.op,
		dbr_type_to_text(args.type),
		args.count);	 
      	ca_signal(args.stat, sprintf_buf);
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
int APIENTRY ca_add_fd_registration(CAFDHANDLER *func, void *arg)
{
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
char APIENTRY *ca_host_name_function(chid chix)
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
int APIENTRY ca_v42_ok(chid chan)
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
 *
 * 	CA_CHANNEL_STATUS
 *
 */
#ifdef vxWorks
int ca_channel_status(int tid)
{
	chid			chix;
	IIU			*piiu;
	struct ca_static 	*pcas;

	pcas = (struct ca_static *) 
		taskVarGet(tid, (int *)&ca_static);

	if (pcas == (struct ca_static *) ERROR)
		return ECA_NOCACTX;

#	define ca_static pcas
		LOCK
#	undef ca_static
	piiu = (struct ioc_in_use *) pcas->ca_iiuList.node.next;
	while(piiu){
		chix = (chid) &piiu->chidlist.node;
		while (chix = (chid) chix->node.next){
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
