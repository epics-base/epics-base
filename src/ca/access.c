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
/*	Environment: VMS, UNIX, vxWorks					*/
/*	Equipment: VAX, SUN, VME					*/
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

static char *sccsId = "$Id$";

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef VMS
#	include		<stsdef.h>
#	include		<ssdef.h>
#	include		<psldef.h>
#	include		<prcdef.h>
#	include 	<descrip.h>
#endif /*VMS*/

#ifdef UNIX
#	include		<signal.h>
#endif /*UNIX*/

#ifdef vxWorks
#	include		<vxWorks.h>
# 	include		<taskLib.h>
# 	include		<task_params.h>
#	include		<sysLib.h>
#	include		<tickLib.h>
#	include		<taskLib.h>
#	include		<taskHookLib.h>
#	include		<logLib.h>
#	include		<usrLib.h>
#	include		<dbgLib.h>
#	include		<taskVarLib.h>
#endif /*vxWorks*/

/*
 * allocate db_access message strings here
 */
#define DB_TEXT_GLBLSOURCE
#include 		<iocinf.h>
#include		<net_convert.h>

#ifdef vxWorks
#include		<taskwd.h>
#endif /*vxWorks*/

/*
 * logistical problems prevent including this file
 */
#ifdef vxWorks
#if 0
#define caClient
#include 		<dbEvent.h>
#endif
#endif


/****************************************************************/
/*	Macros for syncing message insertion into send buffer	*/
/****************************************************************/
#define EXTMSGPTR(PIIU)\
 	((struct extmsg *) &(PIIU)->send.buf[(PIIU)->send.wtix])

#define CAC_ALLOC_MSG(PIIU, EXTSIZE, PPTR) cac_alloc_msg((PIIU), (EXTSIZE), (PPTR))

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
#ifdef __STDC__

LOCAL int cac_alloc_msg(
struct ioc_in_use 	*piiu,
unsigned		extsize,
struct extmsg		**ppMsg
);
LOCAL void 	spawn_repeater();
LOCAL int 	check_for_fp();
LOCAL int 	ca_add_task_variable(struct ca_static *ca_temp);
#ifdef vxWorks
LOCAL void 	ca_task_exit_tcb(WIND_TCB *ptcb);
LOCAL void 	ca_task_exit_tid(int tid);
#else /*vxWorks*/
LOCAL void 	ca_process_exit();
#endif /*vxWorks*/
LOCAL int	issue_get_callback(evid monix);
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
unsigned long			avail,
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
);
LOCAL void ca_put_notify_action(PUTNOTIFY *ppn);
LOCAL void ca_extra_event_labor(void *pArg);
LOCAL void ca_default_exception_handler(struct exception_handler_args args);

LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
struct extmsg		*pmsg,
void			*pext
);

LOCAL void cac_wait_for_flush(IIU *piiu);

#else /*__STDC__*/

LOCAL int 	cac_alloc_msg();
LOCAL void 	spawn_repeater();
LOCAL int	check_for_fp(); 
LOCAL int	ca_add_task_variable();
#ifdef vxWorks
LOCAL void    	ca_task_exit_tcb();
LOCAL void 	ca_task_exit_tid();
#else
LOCAL void 	ca_process_exit();
#endif 
LOCAL void    	issue_get_callback();
LOCAL void    	ca_event_handler();
LOCAL void	ca_pend_io_cleanup();

LOCAL void 	ca_default_exception_handler();
LOCAL void 	create_udp_fd();
LOCAL int 	issue_ca_array_put();
LOCAL void 	ca_put_notify_action();
LOCAL void 	ca_extra_event_labor();
LOCAL int 	cac_push_msg();
LOCAL void 	cac_wait_for_flush();

#endif /*__STDC__*/


/*
 *
 * 	cac_push_msg()
 *
 *	return a pointer to reserved message buffer space or
 *	nill if the message will not fit
 *
 */ 
#ifdef __STDC__
LOCAL int cac_push_msg(
struct ioc_in_use 	*piiu,
struct extmsg		*pmsg,
void			*pext
)
#else
LOCAL int cac_push_msg(piiu, pmsg, pext)
struct ioc_in_use 	*piiu;
struct extmsg		*pmsg;
void			*pext;
#endif
{
	struct extmsg	msg;
	unsigned long	bytesAvailable;
	unsigned long	extsize;
	unsigned long	bytesSent;

	msg = *pmsg;
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
		cac_wait_for_flush(piiu);
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
				extsize);
	assert(bytesSent == extsize);

	UNLOCK;

	return ECA_NORMAL;
}


/*
 * cac_wait_for_flush()
 *
 */
#ifdef __STDC__
LOCAL void cac_wait_for_flush(IIU *piiu)
#else /*__STDC__*/
LOCAL void cac_wait_for_flush(piiu)
IIU 	*piiu;
#endif /*__STDC__*/
{

#ifdef UNIX
	{
		int		flags;
      		struct timeval	itimeout;

		flags = CA_DO_RECVS | CA_DO_SENDS;

      		itimeout.tv_usec 	= LOCALTICKS;	
      		itimeout.tv_sec  	= 0;
		if(post_msg_active){
			ca_select_io(&itimeout, flags);
		}
		else{
			ca_mux_io(&itimeout, flags);
		}
	}
#else /*UNIX*/
	(*piiu->sendBytes)(piiu);
#endif /*UNIX*/
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
#ifdef __STDC__
LOCAL int cac_alloc_msg(
struct ioc_in_use 	*piiu,
unsigned		extsize,
struct extmsg		**ppMsg
)
#else
LOCAL int cac_alloc_msg(piiu, extsize, ppMsg)
struct ioc_in_use 	*piiu;
unsigned		extsize;
struct extmsg		**ppMsg;
#endif
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
#ifdef UNIX
		{
	      		struct timeval	itimeout;
			int		flags;

      			itimeout.tv_usec 	= LOCALTICKS;	
      			itimeout.tv_sec  	= 0;
			flags = CA_DO_RECVS | CA_DO_SENDS;

			UNLOCK;
			if(post_msg_active){
				ca_select_io(&itimeout, flags);
			}
			else{
				ca_mux_io(&itimeout, flags);
			}
			LOCK;
		}
#else /*UNIX*/
		(*piiu->sendBytes)(piiu);
#endif /*UNIX*/

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
#ifdef __STDC__
int ca_task_initialize(void)
#else
int ca_task_initialize()
#endif
{
	int			status;
	struct ca_static	*ca_temp;

	if (!ca_static) {

#ifdef vxWorks
		status = check_for_fp();
		if(status != ECA_NORMAL){
			return status;
		}
#endif /*vxWorks*/

#ifdef UNIX
		/*
		 * dont allow disconnect to terminate process
		 * when running in UNIX enviroment
		 *
		 * allow error to be returned to sendto()
		 * instead of handling disconnect at interrupt
		 */
		signal(SIGPIPE,SIG_IGN);
#endif /*UNIX*/

		if (repeater_installed()==FALSE) {
			spawn_repeater();
		}

		ca_temp = (struct ca_static *) 
				calloc(1, sizeof(*ca_temp));
		if (!ca_temp)
			return ECA_ALLOCMEM;

#ifdef vxWorks
		if (ca_add_task_variable(ca_temp)<0){
			free(ca_temp);
			return ECA_ALLOCMEM;
		}
#else /* vxWorks */
		ca_static = ca_temp;
#endif /* vxWorks */

		ca_static->ca_exception_func = ca_default_exception_handler;
		ca_static->ca_exception_arg = NULL;

		/* record a default user name */
		ca_static->ca_pUserName = localUserName();
		if(!ca_static->ca_pUserName){
			free(ca_static);
			return ECA_ALLOCMEM;
		}

		/* record a default user name */
		ca_static->ca_pLocationName = localLocationName();
		if(!ca_static->ca_pLocationName){
			free(ca_static->ca_pUserName);
			free(ca_static);
			return ECA_ALLOCMEM;
		}

		/* init sync group facility */
		ca_sg_init();

		ellInit(&ca_static->ca_iiuList);
		ellInit(&ca_static->ca_ioeventlist);
		ellInit(&ca_static->ca_free_event_list);
		ellInit(&ca_static->ca_pend_read_list);
		ellInit(&ca_static->ca_pend_write_list);
#ifdef vxWorks
		ellInit(&ca_static->ca_local_chidlist);
		ellInit(&ca_static->ca_dbfree_ev_list);
		ellInit(&ca_static->ca_lcl_buff_list);
		ellInit(&ca_static->ca_taskVarList);
		ellInit(&ca_static->ca_putNotifyQue);
#endif /* vxWorks */
		ca_static->ca_pBucket = bucketCreate(CLIENT_ID_WIDTH);
		if(!ca_static->ca_pBucket)
			abort(0);

#ifdef VMS
		{
			status = lib$get_ef(&io_done_flag);
			if (status != SS$_NORMAL)
				lib$signal(status);
		}
#endif /*VMS*/
#ifdef vxWorks
		{
			char            name[15];
			int             status;

			ca_static->ca_tid = taskIdSelf();

			ca_static->ca_local_ticks = LOCALTICKS;

			ca_static->ca_client_lock = semMCreate(SEM_DELETE_SAFE);
			assert(ca_static->ca_client_lock);
			ca_static->ca_event_lock = semMCreate(SEM_DELETE_SAFE);
			assert(ca_static->ca_event_lock);
			ca_static->ca_putNotifyLock = semMCreate(SEM_DELETE_SAFE);
			assert(ca_static->ca_putNotifyLock);
			ca_static->ca_io_done_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
			assert(ca_static->ca_io_done_sem);
			ca_static->ca_blockSem = 
				semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
			assert(ca_static->ca_blockSem);

			evuser = (void *) db_init_events();
			if (!evuser)
				abort(0);

			status = db_add_extra_labor_event(
					evuser,
					ca_extra_event_labor,
					ca_static);
			if(status)
				abort(0);
			strcpy(name, "EV ");
			strncat(
				name,
				taskName(VXTHISTASKID),
				sizeof(name) - strlen(name) - 1);
			status = db_start_events(
						evuser,
						name,
						ca_import,
						taskIdSelf(),
						-1); /* higher priority */
			if (status != OK)
				abort(0);
		}
#endif /*vxWorks*/
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

	status = broadcast_addr(&ca_static->ca_castaddr);
	if(status != OK){
		return;
	}

	status = alloc_ioc(
		   &ca_static->ca_castaddr,
		   IPPROTO_UDP,
		   &ca_static->ca_piiuCast);
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
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
	if(status<0)
		ca_signal(ECA_INTERNAL,NULL);

	ca_static->recv_tid = status;

#endif
}


/*
 *	spawn_repeater()
 *
 * 	Spawn the repeater task as needed
 */
LOCAL void spawn_repeater()
{

#ifdef UNIX
	{
		int	status;

		/*
		 * use of fork makes ca and repeater images larger than
		 * required but a path is not required in the code.
		 * 
		 * This method of spawn places a path name in the
		 * code so it has been avoided for now:
		 * 	system("/path/repeater &");
		 */
		status = fork();
		if (status < 0)
			SEVCHK(ECA_NOREPEATER, NULL);
		if (status == 0){
			ca_repeater_task();
			exit(0);
		}
	}
#endif
#ifdef vxWorks
	{
		int	status;

		status = taskSpawn(
			   CA_REPEATER_NAME,
			   CA_REPEATER_PRI,
			   CA_REPEATER_OPT,
			   CA_REPEATER_STACK,
			   ca_repeater_task,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL);
		if (status < 0){
			SEVCHK(ECA_NOREPEATER, NULL);
		}
	}
#endif
#ifdef VMS
	{
		static          $DESCRIPTOR(image, 	"EPICS_CA_REPEATER");
		static          $DESCRIPTOR(io, 	"EPICS_LOG_DEVICE");
		static          $DESCRIPTOR(name, 	"CA repeater");
		int		status;
		unsigned long   pid;

		status = sys$creprc(
				    &pid,
				    &image,
				    NULL,	/* input (none) */
				    &io,	/* output */
				    &io,	/* error */
				    NULL,	/* use parents privs */
				    NULL,	/* use default quotas */
				    &name,
				    4,	/* base priority */
				    NULL,
				    NULL,
				    PRC$M_DETACH);
		if (status != SS$_NORMAL){
			SEVCHK(ECA_NOREPEATER, NULL);
#ifdef DEBUG
			lib$signal(status);
#endif
		}
	}
#endif
}



/*
 * CHECK_FOR_FP()
 * 
 * 
 */
#ifdef vxWorks
LOCAL int check_for_fp()
{
	{
		int             options;

		if (taskOptionsGet(taskIdSelf(), &options) == ERROR)
			abort(0);
		if (!(options & VX_FP_TASK)) {
			return ECA_NEEDSFP;
		}
	}
	return ECA_NORMAL;
}
#endif


/*
 * CA_IMPORT()
 * 
 * 
 */
#ifdef vxWorks
#ifdef __STDC__
int ca_import(int tid)
#else
int ca_import(tid)
int             tid;
#endif
{
	int             status;
	struct ca_static *pcas;
	TVIU		*ptviu;

	status = check_for_fp();
	if(status != ECA_NORMAL){
		return status;
	}

	ptviu = calloc(1, sizeof(*ptviu));
	if(!ptviu){
		return ECA_ALLOCMEM;
	}

	pcas = (struct ca_static *) 
		taskVarGet(tid, (int *)&ca_static);
	if (pcas == (struct ca_static *) ERROR){
		free(ptviu);
		return ECA_NOCACTX;
	}

	ca_static = NULL;

	status = taskVarAdd(VXTHISTASKID, (int *)&ca_static);
	if (status == ERROR){
		free(ptviu);
		return ECA_ALLOCMEM;
	}

	ca_static = pcas;

	ptviu->tid = taskIdSelf();
	LOCK;
	ellAdd(&ca_static->ca_taskVarList, &ptviu->node);	
	UNLOCK;

	return ECA_NORMAL;
}
#endif


/*
 * CA_IMPORT_CANCEL()
 */
#ifdef vxWorks
int ca_import_cancel(tid)
int		tid;
{
	int	status;
	TVIU	*ptviu;

	LOCK;
	ptviu = (TVIU *) ca_static->ca_taskVarList.node.next; 
	while(ptviu){
		if(ptviu->tid == tid){
			break;
		}
	}

	if(!ptviu){
		return ECA_NOCACTX;
	}

	ellDelete(&ca_static->ca_taskVarList, &ptviu->node);
	UNLOCK;

	status = taskVarDelete(tid, (void *)&ca_static);
	assert (status == OK);

	return ECA_NORMAL;
}
#endif


/*
 * CA_MODIFY_CLIENT_LOCATION_NAME()
 *
 * Modify or override the default 
 * client location name.
 */
#ifdef __STDC__
int ca_modify_client_location(char *pLocationName)
#else /*__STDC__*/
int ca_modify_client_location(pLocationName)
char *pLocationName;
#endif /*__STDC__*/
{
	char		*pTmp;
	unsigned	size;
	IIU		*piiu;

	INITCHK;

	size = strlen(pLocationName)+1;
	if (size > STRING_LIMIT){
		return ECA_STRTOBIG;
	}

	pTmp = malloc(size);
	if(!pTmp){
		return ECA_ALLOCMEM;
	}

	if(ca_static->ca_pLocationName){
		free(ca_static->ca_pLocationName);
	}
	ca_static->ca_pLocationName = pTmp;

	/*
	 * force null termination
	 */
	strncpy(	
		ca_static->ca_pLocationName, 
		pLocationName, 
		size-1);
	ca_static->ca_pLocationName[size-1]='\0';

	/*
	 * update all servers we are currently
	 * attached to.
	 */
	LOCK;
	piiu = (struct ioc_in_use *) ca_static->ca_iiuList.node.next;
	while(piiu){
		issue_identify_client_location(piiu);
		piiu = (struct ioc_in_use *) piiu->node.next;
	}
	UNLOCK;

	return ECA_NORMAL;
}



/*
 * CA_MODIFY_CLIENT_NAME()
 *
 * Modify or override the default 
 * client (user) name.
 */
#ifdef __STDC__
int ca_modify_client_name(char *pClientName)
#else /*__STDC__*/
int ca_modify_client_name(pClientName)
char *pClientName;
#endif /*__STDC__*/
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
 * CA_ADD_TASK_VARIABLE()
 * 
 */
#ifdef vxWorks
#ifdef __STDC__
LOCAL int ca_add_task_variable(struct ca_static *ca_temp)
#else
LOCAL int ca_add_task_variable(ca_temp)
struct ca_static *ca_temp;
#endif
{
	static char     	ca_installed;
	TVIU			*ptviu;
	int             	status;

#	if DEBUG
		ca_printf("CAC: adding task variable\n");
#	endif

	status = taskVarGet(VXTHISTASKID, (int *)&ca_static);
	if(status == OK){
		ca_printf("task variable already installed?\n");
		return ERROR;
	}

	/*
	 * only one delete hook for all CA tasks
	 */
	if (vxTas(&ca_installed)) {
		/*
		 *
		 * This guarantees that vxWorks's task
		 * variable delete (at task exit) handler runs
		 * after the CA task exit handler. This ensures 
		 * that CA's task variable will still exist 
		 * when it's exit handler runs.
		 *
		 * That is taskVarInit() must run prior to your
		 * taskDeleteHookAdd() if you use a task variable
		 * in a task exit handler.
		 */
#		if DEBUG
			ca_printf("CAC: adding delete hook\n");
#		endif

		status = taskVarInit();
		if (status != OK)
			return ERROR;
		status = taskDeleteHookAdd((FUNCPTR)ca_task_exit_tcb);
		if (status != OK) {
			ca_printf("ca_init_task: could not add CA delete routine\n");
			return ERROR;
		}
	}

	ptviu = calloc(1, sizeof(*ptviu));
	if(!ptviu){
		return ERROR;
	}

	ptviu->tid = taskIdSelf();
	ellAdd(&ca_temp->ca_taskVarList, &ptviu->node);	

	status = taskVarAdd(VXTHISTASKID, (int *)&ca_static);
	if (status != OK){
		free(ptviu);
		return ERROR;
	}

	ca_static = ca_temp;

	return OK;
}
#endif



/*
 *	CA_TASK_EXIT()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
int ca_task_exit
#ifdef __STDC__
(void)
#else
()
#endif
{
#ifdef vxWorks
  	ca_task_exit_tid(VXTHISTASKID);
#else
  	ca_process_exit();
#endif

  	return ECA_NORMAL;
}



/*
 *	CA_TASK_EXIT_TCBX()
 *
 */
#ifdef vxWorks                                                 
#ifdef __STDC__
LOCAL void ca_task_exit_tcb(WIND_TCB *ptcb)
#else
LOCAL void ca_task_exit_tcb(ptcb)
WIND_TCB 	*ptcb;
#endif
{
#	if DEBUG
		ca_printf("CAC: entering the exit handler %x\n", ptcb);
#	endif

	/*
	 * NOTE: vxWorks provides no method at this time 
	 * to get the task id from the ptcb so I am
	 * taking the liberty of using the ptcb as
	 * the task id - somthing which may not be true
	 * on future releases of vxWorks
	 */
  	ca_task_exit_tid((int) ptcb);
}
#endif


/*
 *
 *	CA_TASK_EXIT_TID() / CA_PROCESS_EXIT()
 *	attempts to release all resources alloc to a channel access client
 *
 *	NOTE: on vxWorks if a CA task is deleted or crashes while a 
 *	lock is set then a deadlock will occur when this routine is called.
 *
 */
#ifdef vxWorks
#ifdef __STDC__
LOCAL void ca_task_exit_tid(int tid)
#else
LOCAL void ca_task_exit_tid(tid)
	int             tid;
#endif
#else
LOCAL void ca_process_exit()
#endif
{
	chid            	chix;
	struct ca_static 	*ca_temp;
	evid            	monix;
	IIU			*piiu;
	int             	status;

#	if defined(DEBUG) && defined(vxWorks)
		ca_printf("CAC: entering the exit handler 2 %x\n", tid);
#	endif

#	if defined(vxWorks)

		ca_temp = (struct ca_static *) 
				taskVarGetPatch(tid, &ca_static);

		if (ca_temp == (struct ca_static *) ERROR){
#			ifdef DEBUG
				ca_printf("CAC: task variable lookup failed\n");
#			endif
			return;
		}
#		ifdef DEBUG
			ca_printf(	
				"CAC: exit handler with ca_static = %x\n", 
				ca_temp);
#		endif
#	else
		ca_temp = ca_static;
#	endif


	/* if already in the exit state just NOOP */
#	ifdef vxWorks
		if (!vxTas(&ca_temp->ca_exit_in_progress))
			return;
#	else
		if (ca_temp->ca_exit_in_progress)
			return;
		ca_temp->ca_exit_in_progress = TRUE;
#	endif

	if (ca_temp) {
#		ifdef vxWorks
			ca_printf("ca_task_exit: Removing task %x from CA\n", tid);
#		endif

		/*
		 * force this macro to use ca_temp
		 */
#		define ca_static ca_temp
		LOCK;
#		undef ca_static

		/*
		 * Fist I must stop any source of further activity on vxWorks
		 */

		/*
		 * The main task is prevented from further CA activity by the
		 * LOCK placed above
		 */


		/*
		 * stop socket recv task
		 */
#		ifdef vxWorks
			if(taskIdVerify(ca_temp->recv_tid)==OK){
				taskwdRemove(ca_temp->recv_tid);
				/*
				 * dont do a task delete if the exit handler is
				 * running for this task - it botches vxWorks -
				 */
				if(ca_temp->recv_tid != tid){
					taskSuspend(ca_temp->recv_tid);
				}
			}
#		endif


		/*
		 * Cancel all local events
		 * (and put call backs)
		 */
#		ifdef vxWorks
			chix = (chid) & ca_temp->ca_local_chidlist.node;
			while (chix = (chid) chix->node.next){
				while (monix = (evid) ellGet(&chix->eventq)) {
					status = db_cancel_event(monix + 1);
					if (status == ERROR)
						abort(0);
					free(monix);
				}
				if(chix->ppn){
					CACLIENTPUTNOTIFY *ppn;

					ppn = chix->ppn;
					if(ppn->busy){
						dbNotifyCancel(&ppn->dbPutNotify);
					}
					free(ppn);
				}
			}
#		endif

		/*
		 * cancel task vars for other tasks so this
		 * only runs once
		 *
		 * This is done only after all oustanding events 
		 * are drained so that the event task still has a CA context
		 *
		 * db_close_events() does not require a CA context.
		 */
#		ifdef vxWorks
		 {
			TVIU	*ptviu;
		 	while(ptviu = (TVIU *)ellGet(&ca_temp->ca_taskVarList)){
#ifdef				DEBUG
				ca_printf(
					"Removing tsk var from %x\n",
					ptviu->tid);
#endif
				status = taskVarDelete(
						ptviu->tid, 
						(int *)&ca_static);
				if(status<0){
					ca_printf(
						"tsk var del err %x\n", 
						ptviu->tid);
				}
				free(ptviu);
			}
			if(taskIdVerify(ca_temp->recv_tid)==OK){
				if(ca_temp->recv_tid != tid){
					taskDelete(ca_temp->recv_tid);
				}
			}
		 }
#		endif

		/*
		 * All local events must be canceled prior to closing the
		 * local event facility
		 */
#		ifdef vxWorks
			{
				status = db_close_events(ca_temp->ca_evuser);
				if (status == ERROR)
					ca_signal(
						ECA_INTERNAL, 
						"could not close event facility by id");
			}

			ellFree(&ca_temp->ca_lcl_buff_list);
#		endif

		/*
		 * after activity eliminated
		 * close all sockets before clearing chid blocks and remote
		 * event blocks
		 */
		piiu = (struct ioc_in_use *)
			ca_temp->ca_iiuList.node.next;
		while(piiu){
			if(ca_temp->ca_fd_register_func){
				(*ca_temp->ca_fd_register_func)(
					ca_temp->ca_fd_register_arg,
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
			ca_temp->ca_iiuList.node.next;
		while(piiu){
			while (chix = (chid) ellGet(&piiu->chidlist)) {
				while (monix = (evid) ellGet(&chix->eventq)) {
					free((char *)monix);
				}
				/*
				 * release entries in the bucket table
				 */
				status = bucketRemoveItem(
						ca_temp->ca_pBucket, 
						chix->cid, 
						chix);
				if(status != BUCKET_SUCCESS){
					ca_signal(	
						ECA_INTERNAL, 
						"bad chid at exit");
				}
				free((char *)chix);
			}

			/*
			 * free message body cache
			 */
			if(piiu->pCurData){
				free(piiu->pCurData);
				piiu->pCurData = NULL;
				piiu->curDataMax = 0;
			}

#ifdef VAX
			/*
			 * free put convert cache
			 */
			if(piiu->pPutConvertBuf){
				free(piiu->pPutConvertBuf);
				piiu->pPutConvertBuf = NULL;
				piiu->putConvertBufSize = 0;
			}
#endif
			piiu = (struct ioc_in_use *) piiu->node.next;
		}

		/*
		 * remove local chid blocks, paddr blocks, waiting ev blocks
		 */
#		ifdef vxWorks
			ellFree(&ca_temp->ca_local_chidlist);
			ellFree(&ca_temp->ca_dbfree_ev_list);
#		endif

		/* remove remote waiting ev blocks */
		ellFree(&ca_temp->ca_free_event_list);
		/* remove any pending read blocks */
		ellFree(&ca_temp->ca_pend_read_list);
		/* remove any pending write blocks */
		ellFree(&ca_temp->ca_pend_write_list);
		/* remove any pending io event blocks */
		ellFree(&ca_temp->ca_ioeventlist);

		/* reclaim sync group resources */
		ca_sg_shutdown(ca_temp);

		/*
		 * force this macro to use ca_temp
		 */
#		define ca_static ca_temp
		UNLOCK;
#		undef ca_static

#		if defined(vxWorks)
			assert(semDelete(ca_temp->ca_client_lock)==OK);
			assert(semDelete(ca_temp->ca_event_lock)==OK);
			assert(semDelete(ca_temp->ca_putNotifyLock)==OK);
			assert(semDelete(ca_temp->ca_io_done_sem)==OK);
			assert(semDelete(ca_temp->ca_blockSem)==OK);
#		endif

#		if defined(VMS)
			status = lib$free_ef(&ca_temp->ca_io_done_flag);
			assert(status == SS$_NORMAL)
#		endif

		/*
		 * remove IOCs in use
		 */
		ellFree(&ca_temp->ca_iiuList);

		/*
		 * free user name string
		 */
		if(ca_temp->ca_pUserName){
			free(ca_temp->ca_pUserName);
		}

		/*
		 * free location name string
		 */
		if(ca_temp->ca_pLocationName){
			free(ca_temp->ca_pLocationName);
		}

		/*
		 * free top level bucket
		 */
		status = bucketFree(ca_temp->ca_pBucket);
		if(status != BUCKET_SUCCESS){
			ca_signal(ECA_INTERNAL, "top level bucket free failed");
		}

		/*
		 * free beacon hash table
		 */
		{
			int	len;
			bhe	**ppBHE;
			bhe	*pBHE;

			len = NELEMENTS(ca_temp->ca_beaconHash);
			for(	ppBHE = ca_temp->ca_beaconHash;
				ppBHE < &ca_temp->ca_beaconHash[len];
				ppBHE++){
				pBHE = *ppBHE;
				while(pBHE){
					bhe     *pOld;
					
					pOld = pBHE;
					pBHE = pBHE->pNext;
					free(pOld);
				}
			}
		}

		free((char *)ca_temp);
		ca_static = (struct ca_static *) NULL;

	}
}




/*
 *
 *	CA_BUILD_AND_CONNECT
 *
 *
 */
int ca_build_and_connect
#ifdef __STDC__
(
 char *name_str,
 chtype get_type,
 unsigned long get_count,
 chid * chixptr,
 void *pvalue,
 void (*conn_func) (struct connection_handler_args),
 void *puser
)
#else
(name_str, get_type, get_count, chixptr, pvalue, conn_func, puser)
	char           *name_str;
	chtype         get_type;
	unsigned long  get_count;
	chid           *chixptr;
	void           *pvalue;
	void            (*conn_func) ();
	void           *puser;
#endif
{
	long            status;
	chid            chix;
	int             strcnt;

	/*
	 * make sure that chix is NULL on fail
	 */
	*chixptr = NULL;

	INITCHK;

	if (INVALID_DB_REQ(get_type) && get_type != TYPENOTCONN)
		return ECA_BADTYPE;

	/*
	 * Cant check his count on build since we dont know the native count
	 * yet.
	 */

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
		struct connection_handler_args args;

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
			chix->connection_func = conn_func;
			chix->type = chix->id.paddr->field_type;
			chix->count = chix->id.paddr->no_elements;
			chix->piiu = NULL; /* none */
			chix->state = cs_conn;
			ellInit(&chix->eventq);
			strncpy((char *)(chix + 1), name_str, strcnt);

			/* check for just a search */
			if (get_count && get_type != TYPENOTCONN && pvalue) {
				status = db_get_field(
						&tmp_paddr, 
						get_type, 
						pvalue, 
						get_count, 
						NULL);
				if (status != OK) {
					*chixptr = (chid)  NULL;
					free((char *)chix);
					return ECA_GETFAIL;
				}
			}
			LOCK;
			ellAdd(&local_chidlist, &chix->node);
			UNLOCK;

			if (chix->connection_func) {
				args.chid = chix;
				args.op = CA_OP_CONN_UP;

				LOCKEVENTS;
				(*chix->connection_func) (args);
				UNLOCKEVENTS;
			}
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
	chix->cid = CLIENT_ID_ALLOC;
	status = bucketAddItem(pBucket, chix->cid, chix);
	if(status != BUCKET_SUCCESS){
		*chixptr = (chid) NULL;
		free((char *) chix);
		UNLOCK;
		return ECA_ALLOCMEM;
	}

	chix->puser = puser;
	chix->connection_func = conn_func;
	chix->type = TYPENOTCONN; /* invalid initial type 	 */
	chix->count = 0; 	/* invalid initial count	 */
	chix->id.sid = ~0L;	/* invalid initial server id 	 */

	/* save stuff for build retry if required */
	chix->build_type = get_type;
	chix->build_count = get_count;
	chix->build_value = (void *) pvalue;
	chix->name_length = strcnt;
	chix->state = cs_never_conn;
	ellInit(&chix->eventq);

	/* Save this channels name for retry if required */
	strncpy((char *)(chix + 1), name_str, strcnt);

	chix->piiu = piiuCast;
	ellAdd(&piiuCast->chidlist, &chix->node);

	/*
	 * set the conn tries back to zero so this channel's location
	 * can be found
	 */
	piiuCast->nconn_tries = 0;
	piiuCast->next_retry = CA_CURRENT_TIME;
	piiuCast->retry_delay = CA_RECAST_DELAY;

	UNLOCK;

	build_msg(chix, DONTREPLY);
	if (!chix->connection_func) {
		SETPENDRECV;
		if (VALID_BUILD(chix))
			SETPENDRECV;
	}

	return ECA_NORMAL;

}



/*
 * BUILD_MSG()
 * 
 * NOTE:	*** lock must be applied while in this routine ***
 * 
 */
#ifdef __STDC__
void build_msg(
chid            chix,
int             reply_type
)
#else
void build_msg(chix, reply_type)
chid            chix;
int             reply_type;
#endif
{
	int			status;
	int    			size;
	int    			cmd;
	struct extmsg		*mptr;
	struct ioc_in_use	*piiu;

	piiu = chix->piiu;

	if (VALID_BUILD(chix)) {
		size = chix->name_length + sizeof(struct extmsg);
		cmd = IOC_BUILD;
	} else {
		size = chix->name_length;
		cmd = IOC_SEARCH;
	}

	LOCK;
	status = CAC_ALLOC_MSG(piiu, size, &mptr);
	if(status != ECA_NORMAL){
		ca_printf("%s: %s\n",__FILE__,ca_message(status));
		return;
	}

	mptr->m_cmmd = htons(cmd);
	mptr->m_available = chix->cid;
	mptr->m_type = reply_type;
	mptr->m_count = 0;
	mptr->m_cid = chix->cid;

	if (cmd == IOC_BUILD) {
		/* msg header only on db read req	 */
		mptr++;
		mptr->m_postsize = 0;
		mptr->m_cmmd = htons(IOC_READ_BUILD);
		mptr->m_type = htons(chix->build_type);
		mptr->m_count = htons(chix->build_count);
		mptr->m_available = (int) chix->build_value;
		mptr->m_cid = chix->cid;
	}
	/* 
	 * channel name string - forces a NULL at the end because 
	 * strcnt is always >= strlen + 1 
	 */
	mptr++;
	strncpy((char *)mptr, (char *)(chix + 1), chix->name_length);

	CAC_ADD_MSG(piiu);

	UNLOCK;

	piiu->send_needed = TRUE;
}



/*
 * CA_ARRAY_GET()
 * 
 * 
 */
int ca_array_get
#ifdef __STDC__
(
chtype 		type,
unsigned long 	count,
chid 		chix,
void 		*pvalue
)
#else
(type, count, chix, pvalue)
	chtype          type;
	unsigned long 	count;
	chid            chix;
	void  		*pvalue;
#endif
{
	struct extmsg		hdr;
	int			status;

	CHIXCHK(chix);

	if (INVALID_DB_REQ(type))
		return ECA_BADTYPE;

	if (count > chix->count)
		return ECA_BADCOUNT;

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
	 * 	msg header only on db read req	 
	 */
	hdr.m_cmmd = htons(IOC_READ);
	hdr.m_type = htons(type);
	hdr.m_available = (long) pvalue;
	hdr.m_count = htons(count);
	hdr.m_cid = chix->id.sid;
	hdr.m_postsize = 0;

	status = cac_push_msg(chix->piiu, &hdr, 0);
	if(status != ECA_NORMAL){
		return status;
	}

	SETPENDRECV;

	return ECA_NORMAL;
}



/*
 * CA_ARRAY_GET_CALLBACK()
 * 
 * 
 * 
 */
int ca_array_get_callback
#ifdef __STDC__
(
 chtype type,
 unsigned long count,
 chid chix,
 void (*pfunc) (struct event_handler_args),
 void *arg
)
#else
(type, count, chix, pfunc, arg)
	chtype          type;
	unsigned long 	count;
	chid            chix;
	void            (*pfunc) ();
	void           *arg;
#endif
{
	int             status;
	evid            monix;

	CHIXCHK(chix);

	if (INVALID_DB_REQ(type))
		return ECA_BADTYPE;

	if (count > chix->count)
		return ECA_BADCOUNT;


#ifdef vxWorks
	if (!chix->piiu) {
		struct pending_event ev;

		ev.usr_func = pfunc;
		ev.usr_arg = arg;
		ev.chan = chix;
		ev.type = type;
		ev.count = count;
		ca_event_handler(&ev, chix->id.paddr, NULL, NULL);
		return ECA_NORMAL;
	}
#endif

	LOCK;
	if (!(monix = (evid) ellGet(&free_event_list)))
		monix = (evid) malloc(sizeof(*monix));
	UNLOCK;

	if (monix) {

		memset(monix, 0, sizeof(*monix));
		monix->chan = chix;
		monix->usr_func = pfunc;
		monix->usr_arg = arg;
		monix->type = type;
		monix->count = count;

		LOCK;
		ellAdd(&pend_read_list, &monix->node);
		UNLOCK;

		status = issue_get_callback(monix);
	} else
		status = ECA_ALLOCMEM;

	return status;
}



/*
 *
 *	ISSUE_GET_CALLBACK()
 *	(lock must be on)
 */
#ifdef __STDC__
LOCAL int issue_get_callback(evid monix)
#else
LOCAL int issue_get_callback(monix)
evid   monix;
#endif
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
	if(chix->state != cs_conn){
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
	hdr.m_cmmd = htons(IOC_READ_NOTIFY);
	hdr.m_type = htons(monix->type);
	hdr.m_available = (long) monix;
	hdr.m_count = htons(count);
	hdr.m_postsize = 0;
	hdr.m_cid = chix->id.sid;

	status = cac_push_msg(piiu, &hdr, 0);

	piiu->send_needed = TRUE;

	return status;
}


/*
 *	CA_ARRAY_PUT_CALLBACK()
 *
 */
#ifdef __STDC__
int ca_array_put_callback
(
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue,
void				(*pfunc)(struct event_handler_args),
void				*usrarg
)
#else /*__STDC__*/
int ca_array_put_callback(type,count,chix,pvalue,pfunc,usrarg)
chtype				type;
unsigned long			count;
chid				chix;
void 				*pvalue;
void				(*pfunc)();
void				*usrarg;
#endif /*__STDC__*/
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
  	if(INVALID_DB_FIELD(type))
    		return ECA_BADTYPE;

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
		LOCK;
		monix = (evid) ellGet(&free_event_list);
		UNLOCK;
		if (!monix){
			monix = (evid) malloc(sizeof(*monix));
		}
		if (!monix){
			return ECA_ALLOCMEM;
		}

		memset(monix,0,sizeof(*monix));
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
			(unsigned long)monix, 
			type, 
			count, 
			chix, 
			pvalue);
	if(status != ECA_NORMAL){
		if(chix->piiu){
			LOCK;
			ellAdd(&free_event_list, &monix->node);
			UNLOCK;
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
 * 	CA_EXTRA_EVENT_LABOR
 */
#ifdef vxWorks
LOCAL void ca_extra_event_labor(void *pArg)
{
	int			status;
	CACLIENTPUTNOTIFY	*ppnb;
	struct ca_static 	*pcas;
	struct event_handler_args args;

	pcas = pArg;

	while(TRUE){
                /*
                 * independent lock used here in order to
                 * avoid any possibility of blocking
                 * the database (or indirectly blocking
                 * one client on another client).
                 */
        	semTake(pcas->ca_putNotifyLock, WAIT_FOREVER);
		ppnb = (CACLIENTPUTNOTIFY *)ellGet(&pcas->ca_putNotifyQue);
        	semGive(pcas->ca_putNotifyLock);

		/*
                 * break to loop exit
                 */
                if(!ppnb){
                        break;
                }

		/*
 		 * setup arguments and call user's function 
		 */
		args.usr = ppnb->caUserArg;
		args.chid = ppnb->dbPutNotify.usrPvt;
		args.type = ppnb->dbPutNotify.dbrType;
		args.count = ppnb->dbPutNotify.nRequest;
		args.dbr = NULL;
		if(ppnb->dbPutNotify.status){
			if(ppnb->dbPutNotify.status == S_db_Blocked){
				args.status = ECA_PUTCBINPROG;
			}
			else{
				args.status = ECA_PUTFAIL;
			}
		}
		else{
			args.status = ECA_NORMAL;
		}

		LOCKEVENTS;
		(*ppnb->caUserCallback) (args);
		UNLOCKEVENTS;	

		ppnb->busy = FALSE;
	}

        /*
         * wakeup the TCP thread if it is waiting for a cb to complete
         */
        status = semGive(pcas->ca_blockSem);
        if(status != OK){
                logMsg("CA block sem corrupted\n",
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
        }
	
}
#endif /*vxWorks*/


/*
 *	CA_ARRAY_PUT()
 *
 *
 */
int ca_array_put
#ifdef __STDC__
(
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
)
#else /*__STDC__*/
(type,count,chix,pvalue)
chtype				type;
unsigned long			count;
chid				chix;
void 				*pvalue;
#endif /*__STDC__*/
{
	/*
	 * valid channel id test
	 */
  	CHIXCHK(chix);

	/*
	 * compound types not allowed
	 */
  	if(INVALID_DB_FIELD(type))
    		return ECA_BADTYPE;

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

	return issue_ca_array_put(IOC_WRITE, ~0L, type, count, chix, pvalue);
}


/*
 * issue_ca_array_put()
 */
#ifdef __STDC__
static int issue_ca_array_put
(
unsigned			cmd,
unsigned long			avail,
chtype				type,
unsigned long			count,
chid				chix,
void				*pvalue
)
#else /*__STDC__*/
static int issue_ca_array_put(cmd, avail, type, count, chix, pvalue)
unsigned			cmd;
unsigned long			avail;
chtype				type;
unsigned long			count;
chid				chix;
void 				*pvalue;
#endif /*__STDC__*/
{ 
	int			status;
	struct ioc_in_use	*piiu;
	struct extmsg		hdr;
  	int  			postcnt;
#ifdef VAX
  	void			*pdest;
#endif
  	unsigned		size_of_one;
  	int 			i;


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


	LOCK;
#ifdef VAX
	if(piiu->putConvertBufSize<postcnt){
		if(piiu->pPutConvertBuf){
			free(piiu->pPutConvertBuf);
		}
		piiu->putConvertBufSize = 0;
		piiu->pPutConvertBuf = malloc(postcnt);
		if(!piiu->pPutConvertBuf){
			UNLOCK;
			return ECA_ALLOCMEM;
		}
		piiu->putConvertBufSize = postcnt;
	}
	pdest = piiu->pPutConvertBuf;

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
#if DBR_INT != DBR_SHORT
      		case	DBR_INT:
#endif /*DBR_INT != DBR_SHORT*/
        		*(short *)pdest = htons(*(short *)pvalue);
        		break;

      		case	DBR_FLOAT:
        		/* most compilers pass all floats as doubles */
        		htonf(pvalue, pdest);
        		break;

      		case	DBR_STRING:
			/*
			 * string size checked above
			 */
          		strcpy(pdest,pvalue);
        		break;

      		case	DBR_DOUBLE: /* no cvrs for double yet */
      		default:
			UNLOCK;
        		return ECA_BADTYPE;
      		}
      		(char *) pdest += size_of_one;
      		(char *) pvalue += size_of_one;
    	}
	
	pvalue = pdest;
#endif /*VAX*/

    	hdr.m_cmmd 		= htons(cmd);
   	hdr.m_type		= htons(type);
    	hdr.m_count 		= htons(count);
    	hdr.m_cid 		= chix->id.sid;
    	hdr.m_available 	= avail;
	hdr.m_postsize 		= postcnt;

	status = cac_push_msg(piiu, &hdr, pvalue);

	UNLOCK;
	piiu->send_needed = TRUE;

  	return status;
}


/*
 *	Specify an event subroutine to be run for connection events
 *
 */
int ca_change_connection_event
#ifdef __STDC__
(
chid		chix,
void 		(*pfunc)(struct connection_handler_args)
)
#else
(chix, pfunc)
chid		chix;
void		(*pfunc)();
#endif
{

  	INITCHK;
  	LOOSECHIXCHK(chix);

	if(chix->connection_func == pfunc)
  		return ECA_NORMAL;

  	LOCK;
  	if(chix->type == TYPENOTCONN){
		if(!chix->connection_func && chix->state==cs_never_conn){
    			CLRPENDRECV(FALSE);
    			if(VALID_BUILD(chix)) 
    				CLRPENDRECV(FALSE);
		}
		if(!pfunc){
    			SETPENDRECV;
    			if(VALID_BUILD(chix)) 
    				SETPENDRECV;
		}
	}
  	chix->connection_func = pfunc;
  	UNLOCK;

  	return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for asynch exceptions
 *
 */
int ca_add_exception_event
#ifdef __STDC__
(
void 		(*pfunc)(struct exception_handler_args),
void 		*arg
)
#else
(pfunc,arg)
void				(*pfunc)();
void				*arg;
#endif
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
int ca_add_io_event
#ifdef __STDC__
(
void 		(*ast)(),
void 		*astarg
)
#else
(ast,astarg)
void		(*ast)();
void		*astarg;
#endif
{
  register struct pending_io_event	*pioe;

  INITCHK;

  if(pndrecvcnt<1)
    (*ast)(astarg);
  else{
    pioe = (struct pending_io_event *) malloc(sizeof(*pioe));
    if(!pioe)
      return ECA_ALLOCMEM;
    memset(pioe,0,sizeof(*pioe));
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
int ca_add_masked_array_event
#ifdef __STDC__
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
#else
(type,count,chix,ast,astarg,p_delta,n_delta,timeout,monixptr,mask)
chtype				type;
unsigned long			count;
chid				chix;
void				(*ast)();
void				*astarg;
ca_real				p_delta;
ca_real				n_delta;
ca_real				timeout;
evid				*monixptr;
long				mask;
#endif
{
  evid		monix;
  int		status;
  int		size;

  INITCHK;
  LOOSECHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  if(count > chix->count && chix->type != TYPENOTCONN)
    return ECA_BADCOUNT;

  if(!mask)
    return ECA_BADMASK;

  LOCK;

  /*	event descriptor	*/
# ifdef vxWorks
  {
    static int			dbevsize;

    if(!chix->piiu){

      if(!dbevsize)
        dbevsize = db_sizeof_event_block();

      size = sizeof(*monix)+dbevsize;
      if(!(monix = (evid)ellGet(&dbfree_ev_list))){
        monix = (evid)malloc(size);
      }
    }
    else{
      size = sizeof(*monix);
      if(!(monix = (evid)ellGet(&free_event_list))){
        monix = (evid)malloc(size);
      }
    }
  }
# else
  size = sizeof(*monix);
  if(!(monix = (evid)ellGet(&free_event_list)))
    monix = (evid) malloc(size);
# endif

  if(!monix){
    UNLOCK;
    return ECA_ALLOCMEM;
  }

  memset(monix,0,size);

  /* they dont have to supply one if they dont want to */
  if(monixptr)
    *monixptr = monix;

  monix->chan		= chix;
  monix->usr_func	= ast;
  monix->usr_arg	= astarg;
  monix->type		= type;
  monix->count		= count;
  monix->p_delta	= p_delta;
  monix->n_delta	= n_delta;
  monix->timeout	= timeout;
  monix->mask		= mask;

# ifdef vxWorks
  {

    if(!chix->piiu){
      status = db_add_event(	evuser,
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
	Place in the channel list 
	- do it after db_add_event so there
	is no chance that it will be deleted 
	at exit before it is completely created
      */
      ellAdd(&chix->eventq, &monix->node);

      /* 
	force event to be called at least once
      	return warning msg if they have made the queue to full 
	to force the first (untriggered) event.
      */
      if(db_post_single_event(monix+1)==ERROR){
	UNLOCK;
        return ECA_OVEVFAIL;
      }

      UNLOCK;
      return ECA_NORMAL;
    } 
  }
# endif

  /* It can be added to the list any place if it is remote */
  /* Place in the channel list */
  ellAdd(&chix->eventq, &monix->node);

  UNLOCK;

  return ca_request_event(monix);
}


/*
 *	CA_REQUEST_EVENT()
 */
#ifdef __STDC__
int ca_request_event(evid monix)
#else
int ca_request_event(monix)
evid            monix;
#endif
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
	msg.m_header.m_available = (long) monix;
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
#ifdef __STDC__
LOCAL void ca_event_handler(
evid 		monix,
struct db_addr	*paddr,
int		hold,
void		*pfl
)
#else
LOCAL void ca_event_handler(monix, paddr, hold, pfl)
evid 		monix;
struct db_addr	*paddr;
int		hold;
void		*pfl;
#endif
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
int ca_clear_event
#ifdef __STDC__
(evid monix)
#else
(monix)
evid   monix;
#endif
{
	int		status;
	chid   		chix = monix->chan;
	struct extmsg 	hdr;

	/*
	 * is it a valid channel ?
	 */
	LOOSECHIXCHK(chix);

	/*
	 * is it a valid monitor id
	 */
	status = ellFind(&chix->eventq, &monix->node);
	if(status==ERROR){
		return ECA_BADMONID;
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
		hdr.m_available = (long) monix;
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
int ca_clear_channel
#ifdef __STDC__
(chid chix)
#else
(chix)
chid   chix;
#endif
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
		ellConcat(&free_event_list, &chix->eventq);
		ellDelete(&piiu->chidlist, &chix->node);
		status = bucketRemoveItem(pBucket, chix->cid, chix);
		if(status != BUCKET_SUCCESS){
			ca_signal(	
				ECA_INTERNAL, 
				"bad id at channel delete");
		}
		free((char *) chix);
		UNLOCK;
		return ECA_NORMAL;
	}

	/*
	 * clear events and all other resources for this chid on the
	 * IOC
	 */

	/* msg header	 */
	hdr.m_cmmd = htons(IOC_CLEAR_CHANNEL);
	hdr.m_available = (int) chix;
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
 * NOTE: This call not implemented with select on VMS or vxWorks due to its
 * poor implementation in those environments.
 * 
 * Wallangong's SELECT() does not return early if a timeout is specified and IO
 * occurs before the end of the timeout. Above call works because of the
 * short timeout interval.
 * 
 * Another wallongong bug will cause problems if the timeout in secs is
 * specified large. I have fixed this by keeping track of large timeouts
 * myself.
 * 
 */
/************************************************************************/
/*	This routine pends waiting for channel events and calls the 	*/
/*	timeout is specified as 0 infinite timeout is assumed.		*/
/*	if the argument early is specified TRUE then CA_NORMAL is 	*/
/*	returned early (prior to timeout experation) when outstanding 	*/
/*	IO completes.							*/
/*	ca_flush_io() is called by this routine.			*/
/************************************************************************/
#ifdef __STDC__
int ca_pend(ca_real timeout, int early)
#else
ca_pend(timeout, early)
ca_real			timeout;
int			early;
#endif
{
  	time_t 		beg_time;

  	INITCHK;

	if(EVENTLOCKTEST){
    		return ECA_EVDISALLOW;
	}

	manage_conn(TRUE);

  	/*	
	 * Flush the send buffers
	 */
    	if(pndrecvcnt<1 && early){
		ca_flush_io();
        	return ECA_NORMAL;
	}

	/*
	 * quick exit if a poll
	 */
  	if((timeout*SYSFREQ)<LOCALTICKS && timeout != 0.0){

		if(pndrecvcnt>0 && early){
			ca_pend_io_cleanup();
		}

		/*
		 * also takes care of outstanding recvs
		 * under UNIX
		 */
		ca_flush_io();

    		if(pndrecvcnt<1 && early){
        		return ECA_NORMAL;
		}
		else{
      	 		return ECA_TIMEOUT;
		}
  	}

  	beg_time = time(NULL);

  	while(TRUE){
#		ifdef UNIX
    		{
      			struct timeval	itimeout;

      			itimeout.tv_usec 	= LOCALTICKS;	
      			itimeout.tv_sec  	= 0;
			ca_mux_io(&itimeout, CA_DO_RECVS | CA_DO_SENDS);
    		}
#		else /*UNIX*/
  			ca_flush_io();
#	 	endif /*UNIX*/	

#  		ifdef vxWorks
			semTake(io_done_sem, LOCALTICKS);
#  		endif /*vxWorks*/	

#    		ifdef VMS
    		{
      			int 		status; 
      			unsigned int 	systim[2]={-LOCALTICKS,~0};
	
      			status = sys$setimr(
				io_done_flag, 
				systim, 
				NULL, 
				MYTIMERID, 
				NULL);
      			if(status != SS$_NORMAL)
        			lib$signal(status);
      			status = sys$waitfr(io_done_flag);
      			if(status != SS$_NORMAL)
        			lib$signal(status);
    		}   
#		endif /*VMS*/


    		if(pndrecvcnt<1 && early)
        		return ECA_NORMAL;

    		if(timeout != 0.0){
      			if(timeout < time(NULL)-beg_time){
				if(early){
					ca_pend_io_cleanup();
				}
				ca_flush_io();
        			return ECA_TIMEOUT;
      			}
  		}    

		manage_conn(TRUE);
	}
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
int ca_flush_io
#ifdef __STDC__
(void)
#else
()
#endif
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
#		ifdef UNIX
			ca_mux_io(&timeout, CA_DO_RECVS | CA_DO_SENDS);
#		endif /*UNIX*/

		/*
		 * wait for all buffers to flush
		 */
		pending = FALSE;
		LOCK;
		for(	piiu = (IIU *) iiuList.node.next;
			piiu;
			piiu = (IIU *) piiu->node.next){

			bytesPending = cacRingBufferReadSize(&piiu->send, FALSE);
			if(bytesPending != 0){
				pending = TRUE;
#				ifndef UNIX
					(*piiu->sendBytes)(piiu);
#				endif /*UNIX*/
			}
		}
		UNLOCK;

      		timeout.tv_usec = LOCALTICKS;	
		timeout.tv_sec = 0;
	}

  	return ECA_NORMAL;
}


/*
 *	CA_TEST_IO ()
 *
 */
int ca_test_io()
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
#ifdef __STDC__
void ca_signal(long ca_status,char *message)
#else
void ca_signal(ca_status,message)
long		ca_status;
char		*message;
#endif
{
	ca_signal_with_file_and_lineno(ca_status, message, NULL, 0);
}


/*
 * ca_signal_with_file_and_lineno()
 */
#ifdef __STDC__
void ca_signal_with_file_and_lineno(
long		ca_status, 
char		*message, 
char		*pfilenm, 
int		lineno)
#else
void ca_signal_with_file_and_lineno(ca_status,message,pfilenm,lineno)
int		ca_status;
char		*message;
char		*pfilenm;
int		lineno;
#endif
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
"    Message: [%s]\n", ca_message_text[CA_EXTRACT_MSG_NO(ca_status)]);

  if(message)
    ca_printf(
"    Severity: [%s] Context: [%s]\n", 
	severity[CA_EXTRACT_SEVERITY(ca_status)],
	message);
  else
    ca_printf(
"    Severity: [%s]\n", severity[CA_EXTRACT_SEVERITY(ca_status)]);

  if(pfilenm){
	ca_printf(
"    Source File: [%s] Line Number: [%d]\n",
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
#   ifdef VMS
      lib$signal(0);
#   endif
    abort(0);
  }

  ca_printf(
"..................................................................\n");


}





/*
 *	CA_BUSY_MSG()
 *
 */
#ifdef __STDC__
void ca_busy_message(struct ioc_in_use *piiu)
#else
void ca_busy_message(piiu)
struct ioc_in_use *piiu;
#endif
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
#ifdef __STDC__
void ca_ready_message(struct ioc_in_use *piiu)
#else
void ca_ready_message(piiu)
struct ioc_in_use *piiu;
#endif
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
	hdr.m_cmmd = htons(IOC_EVENTS_ON);
	
	cac_push_msg(piiu, &hdr, NULL);

	piiu->send_needed = TRUE;
}


/*
 *
 * NOOP_MSG (lock must be on)
 * 
 * now allows variable size NOOP message
 *
 */
#ifdef __STDC__
void noop_msg(struct ioc_in_use *piiu)
#else /*__STDC__*/
void noop_msg(piiu)
struct ioc_in_use 	*piiu;
#endif /*__STDC__*/
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
 * ISSUE_IDENTIFY_CLIENT_LOCATION (lock must be on)
 * 
 */
#ifdef __STDC__
void issue_identify_client_location(struct ioc_in_use *piiu)
#else /*__STDC__*/
void issue_identify_client_location(piiu)
struct ioc_in_use	*piiu;
#endif /*__STDC__*/
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
	pName =	ca_static->ca_pLocationName, 
	size = strlen(pName)+1;
	hdr = nullmsg;
	hdr.m_cmmd = htons(IOC_CLIENT_LOCATION);
	hdr.m_postsize = size;
	
	cac_push_msg(piiu, &hdr, pName);

	piiu->send_needed = TRUE;

	return;
}


/*
 * ISSUE_IDENTIFY_CLIENT (lock must be on)
 * 
 */
#ifdef __STDC__
void issue_identify_client(struct ioc_in_use *piiu)
#else /*__STDC__*/
void issue_identify_client(piiu)
struct ioc_in_use	*piiu;
#endif /*__STDC__*/
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
#ifdef __STDC__
void issue_claim_channel(struct ioc_in_use *piiu, chid pchan)
#else
void issue_claim_channel(piiu, pchan)
struct ioc_in_use	*piiu;
chid 			pchan;
#endif
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
	hdr.m_cmmd = htons(IOC_CLAIM_CIU);
	hdr.m_cid = pchan->id.sid;

	/*
	 * The available field is used (abused)
	 * here to communicate the miner version number
	 * starting with CA 4.1.
	 */
	hdr.m_available = CA_MINOR_VERSION;

	cac_push_msg(piiu, &hdr, NULL);

	piiu->send_needed = TRUE;
}



/*
 *
 *	Default Exception Handler
 *
 *
 */
#ifdef __STDC__
LOCAL void ca_default_exception_handler(struct exception_handler_args args)
#else
LOCAL void ca_default_exception_handler(args)
struct exception_handler_args args;
#endif
{
 
      	/* 
	 * NOTE:
	 * I should print additional diagnostic info when time permits......
      	 */

      ca_signal(args.stat, args.ctx);
}



/*
 *	CA_ADD_FD_REGISTRATION
 *
 *	call their function with their argument whenever 
 *	a new fd is added or removed
 *	(for a manager of the select system call under UNIX)
 *
 */
#ifdef __STDC__
int ca_add_fd_registration(CAFDHANDLER *func, void *arg)
#else
int ca_add_fd_registration(func, arg)
CAFDHANDLER	*func;
void		*arg;
#endif
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
#ifdef VMS
	SEVCHK(ECA_DEFUNCT, "you must have a VMS share image mismatch\n");
#else
	SEVCHK(ECA_DEFUNCT, NULL);
#endif
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
#ifdef __STDC__
char *ca_host_name_function(chid chix)
#else /*__STDC__*/
char *ca_host_name_function(chix)
chid	chix;
#endif /*__STDC__*/
{
	IIU	*piiu;

	piiu = chix->piiu;

	if(!piiu){
		return "<local host>";
	}
	return piiu->host_name_str;
}




/*
 *
 * 	CA_CHANNEL_STATUS
 *
 */
#ifdef vxWorks
#ifdef __STDC__
int ca_channel_status(int tid)
#else /*__STDC__*/
int ca_channel_status(tid)
int tid;
#endif /*__STDC__*/
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
