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
/*	070191	joh	allways use memcpy in ca_put			*/
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

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

#if defined(VMS)
#	include		stsdef.h
#	include		ssdef.h
#	include		psldef.h
#	include		prcdef.h
#	include 	descrip.h
#elif defined(UNIX)
#elif defined(vxWorks)
#	include		<vxWorks.h>
# 	include		<taskLib.h>
# 	include		<task_params.h>
#else
	@@@@ dont compile @@@@
#endif

/*
 * allocate db_access message strings here
 */
#define DB_TEXT_GLBLSOURCE
#include 		<cadef.h>
#include		<net_convert.h>

#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>


/****************************************************************/
/*	Macros for syncing message insertion into send buffer	*/
/****************************************************************/
#define EXTMSGPTR(PIIU)\
 	((struct extmsg *) &(PIIU)->send->buf[(PIIU)->send->stk])

#define CAC_ALLOC_MSG(PIIU, EXTSIZE) cac_alloc_msg((PIIU), (EXTSIZE))
/*
 * Make sure string is aligned for the most restrictive architecture
 * VAX 		none required
 * 680xx	binary rounding
 * SPARC	octal rounding 
 */
#define CAC_ADD_MSG(PIIU) \
{ \
	struct extmsg *mp = EXTMSGPTR(PIIU); \
	mp->m_postsize = CA_MESSAGE_ALIGN(mp->m_postsize); \
	(PIIU)->send->stk += sizeof(struct extmsg) + mp->m_postsize; \
	mp->m_postsize = htons(mp->m_postsize); \
}




/****************************************************************/
/*	Macros for error checking				*/
/****************************************************************/
/*
	valid type check filters out disconnected channel
*/

#define CHIXCHK(CHIX) \
{ \
  	register unsigned short iocix = (CHIX)->iocix; \
  	if(	iocix == BROADCAST_IIU || \
		(iocix >= nxtiiu && iocix != LOCAL_IIU) || \
		INVALID_DB_REQ((CHIX)->type)  ) \
    	return ECA_BADCHID; \
}

/* type to detect them using deallocated channel in use block */
#define TYPENOTINUSE (-2)

/* allow them to add monitors to a disconnected channel */
#define LOOSECHIXCHK(CHIX) \
{ \
  	register unsigned short iocix = CHIX->iocix; \
  	if( 	(iocix >= nxtiiu && iocix != LOCAL_IIU) || \
		(CHIX)->type==TYPENOTINUSE ) \
    	return ECA_BADCHID; \
}

#define INITCHK \
if(!ca_static) \
  	ca_task_initialize();


static struct extmsg	nullmsg;

/*
 * local functions
 */
void 		ca_default_exception_handler();
void		*db_init_events();
void		ca_default_exception_handler();
void 		spawn_repeater();
void		check_for_fp();
void    	issue_get_callback();
void    	ca_event_handler();
void 		ca_pend_io_cleanup();
int		ca_add_task_variable();
int		ca_import();
struct extmsg 	*cac_alloc_msg();
#ifdef vxWorks
void 		ca_task_exit_tid();
void    	ca_task_exit_tcb();
#else
void 		ca_process_exit();
#endif


/*
 *
 * 	cac_alloc_msg()
 *
 *	return a pointer to reserved message buffer space or
 *	nill if the message will not fit
 *
 *
 */ 
struct extmsg *cac_alloc_msg(piiu, extsize)
struct ioc_in_use 	*piiu;
unsigned		extsize;
{
	unsigned 	msgsize = sizeof(*cac_alloc_msg())+extsize;
	struct extmsg	*pmsg;

  	if(piiu->send->stk+msgsize > piiu->max_msg){
    		cac_send_msg(); 
		if(piiu->send->stk+msgsize > piiu->max_msg){
			return NULL;
		}
	}
	pmsg = (struct extmsg *) (piiu->send->buf + piiu->send->stk);
	pmsg->m_postsize = extsize;
	return pmsg;
}


/*
 *
 *	CA_TASK_INITIALIZE
 *
 *
 */
ca_task_initialize
#ifdef VAXC
(void)
#else
()
#endif
{
	int	status;

	if (!ca_static) {

		if (repeater_installed()==FALSE) {
			spawn_repeater();
		}
#ifdef vxWorks
		check_for_fp();
		if (ca_add_task_variable()<0)
			return ECA_ALLOCMEM;
#endif

		ca_static = (struct ca_static *) calloc(1, sizeof(*ca_static));
		if (!ca_static)
			abort();

		ca_static->ca_exception_func = ca_default_exception_handler;
		ca_static->ca_exception_arg = NULL;

		status = broadcast_addr(&ca_static->ca_castaddr);
		if(status == OK){
			ca_static->ca_cast_available = TRUE;
			SEVCHK(ECA_NOCAST, NULL);
		}

#if defined(VMS)
		{
			status = lib$get_ef(&io_done_flag);
			if (status != SS$_NORMAL)
				lib$signal(status);
		}
#elif defined(vxWorks)
		{
			char            name[15];
			int             status;

			ca_static->ca_tid = taskIdSelf();

			FASTLOCKINIT(&client_lock);
			FASTLOCKINIT(&event_lock);
#ifdef V5_vxWorks
			io_done_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
#else
			io_done_sem = semCreate();
#endif
			if(!io_done_sem){
				abort();
			}

			evuser = (void *) db_init_events();
			if (!evuser)
				abort();

			strcpy(name, "EV ");
			strncat(
				name,
				taskName(VXTHISTASKID),
				sizeof(name) - strlen(name) - 1);
			status = db_start_events(
						 evuser,
						 name,
						 ca_import,
						 taskIdSelf());
			if (status != OK)
				abort();
		}
#elif defined(UNIX)
#else
		@@@@ dont compile in this case @@@@
#endif

	}
	return ECA_NORMAL;
}



/*
 *	spawn_repeater()
 *
 * 	Spawn the repeater task as needed
 */
static void 
spawn_repeater()
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
		if (status == 0)
			ca_repeater_task();
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
			   ca_repeater_task);
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
static void
check_for_fp()
{
	{
		int             options;

		if (taskOptionsGet(taskIdSelf(), &options) == ERROR)
			abort();
		if (!(options & VX_FP_TASK)) {
			ca_signal(ECA_NEEDSFP, NULL);

			if (taskOptionsSet(
				taskIdSelf(), 
				VX_FP_TASK, 
				VX_FP_TASK) == ERROR)
			{
				abort();
			}
		}
	}
}
#endif


/*
 * CA_IMPORT()
 * 
 * 
 */
#ifdef vxWorks
ca_import(tid)
	int             tid;
{
	int             status;
	struct ca_static *pcas;

	pcas = (struct ca_static *) 
		taskVarGet(tid, &ca_static);

	if (pcas == (struct ca_static *) ERROR)
		return ECA_NOCACTX;

	status = taskVarAdd(VXTHISTASKID, &ca_static);
	if (status == ERROR)
		abort();

	ca_static = pcas;

	check_for_fp();

	return ECA_NORMAL;
}
#endif





/*
 * CA_ADD_TASK_VARIABLE()
 * 
 */
#ifdef vxWorks
static int
ca_add_task_variable()
{
	static char     ca_installed;
	int             status;

#	if DEBUG
		ca_printf("CAC: adding task variable\n");
#	endif

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

#		ifdef V5_vxWorks
			status = taskVarInit();
			if (status != OK)
				return ERROR;
#		endif
		status = taskDeleteHookAdd(ca_task_exit_tcb);
		if (status != OK) {
			logMsg("ca_init_task: could not add CA delete routine\n");
			return ERROR;
		}
	}

	status = taskVarAdd(VXTHISTASKID, &ca_static);
	if (status != OK)
		return ERROR;

	return OK;
}
#endif



/*
 *	CA_TASK_EXIT()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
ca_task_exit
#ifdef VAXC
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
#ifdef V5_vxWorks
static void
ca_task_exit_tcb(ptcb)
WIND_TCB 	*ptcb;
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
#else
static void
ca_task_exit_tcb(ptcbx)
TCBX 	*ptcbx;
{
	ca_task_exit_tid((int) ptcbx->taskId);
}
#endif
#endif


/*
 *
 *	CA_TASK_EXIT_TID() / CA_PROCESS_EXIT()
 *	attempts to release all resources alloc to a channel access client
 *
 *	NOTE: on vxWorks if a CA task is deleted or crashes while a 
 *	lock is set then a deadlock will occur when this routine is called.
 *
static 
 */
void
#ifdef vxWorks
ca_task_exit_tid(tid)
	int             tid;
#else
ca_process_exit()
#endif
{
	int             	i;
	chid            	chix;
	struct ca_static 	*ca_temp;
	evid            	monix;
#ifdef vxWorks
	int             	status;
#endif

#	ifdef DEBUG
		ca_printf("CAC: entering the exit handler 2 %x\n", tid);
#	endif

#	if defined(vxWorks)

#		ifdef V5_vxWorks
			ca_temp = (struct ca_static *) 
				taskVarGetPatch(tid, &ca_static);
#		else
			ca_temp = (struct ca_static *) 
				taskVarGet(tid, &ca_static);
#		endif

		if (ca_temp == (struct ca_static *) ERROR){
#			if DEBUG
				ca_printf("CAC: task variable lookup failed\n");
#			endif
			return;
		}
#		if DEBUG
			ca_printf(	"CAC: exit handler with ca_static = %x\n", 
				ca_static);
#		endif
#	else
		ca_temp = ca_static;
#	endif

	/* if allready in the exit state just NOOP */
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
			logMsg("ca_task_exit: Removing task %x from CA\n", tid);
#		endif

		LOCK;
		/*
		 * Fist I must stop any source of further activity on vxWorks
		 */

		/*
		 * The main task is prevented from further CA activity by the
		 * LOCK placed above
		 */

		/*
		 * stop socket recv tasks
		 */
#		ifdef vxWorks
			for (i = 0; i < ca_temp->ca_nxtiiu; i++) {
				/*
				 * dont do a task delete if the exit handler is
				 * running for this task - it botches vxWorks -
				 */
				if (tid != ca_temp->ca_iiu[i].recv_tid)
					taskDelete(ca_temp->ca_iiu[i].recv_tid);
			}
#		endif

		/*
		 * Cancel all local events
		 */
#		ifdef vxWorks
			chix = (chid) & ca_temp->ca_local_chidlist;
			while (chix = (chid) chix->node.next){
				while (monix = (evid) lstGet(&chix->eventq)) {
					status = db_cancel_event(monix + 1);
					if (status == ERROR)
						abort();
					free(monix);
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

			lstFree(&ca_temp->ca_lcl_buff_list);
#		endif

		/*
		 * after activity eliminated:
		 */
		/*
		 * close all sockets before clearing chid blocks and remote
		 * event blocks
		 */
		for (i = 0; i < ca_temp->ca_nxtiiu; i++) {
			if (ca_temp->ca_iiu[i].conn_up)
				if (socket_close(ca_temp->ca_iiu[i].sock_chan) < 0)
					ca_signal(
						ECA_INTERNAL, 
						"Corrupt iiu list- at close");
			free((char *)ca_temp->ca_iiu[i].send);
			free((char *)ca_temp->ca_iiu[i].recv);
		}

		/*
		 * remove remote chid blocks and event blocks
		 */
		for (i = 0; i < ca_temp->ca_nxtiiu; i++) {
			while (chix = (chid) lstGet(&ca_temp->ca_iiu[i].chidlist)) {
				while (monix = (evid) lstGet(&chix->eventq)) {
					free((char *)monix);
				}
				free((char *)chix);
			}
		}

		/*
		 * remove local chid blocks, paddr blocks, waiting ev blocks
		 */
#		ifdef vxWorks
			while (chix = (chid) lstGet(&ca_temp->ca_local_chidlist))
				free((char *)chix);
			lstFree(&ca_temp->ca_dbfree_ev_list);
#		endif

		/* remove remote waiting ev blocks */
		lstFree(&ca_temp->ca_free_event_list);
		/* remove any pending read blocks */
		lstFree(&ca_temp->ca_pend_read_list);

		UNLOCK;

#		if defined(vxWorks)
			if(FASTLOCKFREE(&ca_temp->ca_client_lock) < 0)
				ca_signal(ECA_INTERNAL, "couldnt free memory");
			if(FASTLOCKFREE(&ca_temp->ca_event_lock) < 0)
				ca_signal(ECA_INTERNAL, "couldnt free memory");
#			ifdef V5_vxWorks
				status = semDelete(ca_temp->ca_io_done_sem);
				if(status < 0){
					ca_signal(
						ECA_INTERNAL, 
						"couldnt free sem");
				}
#			else
				semDelete(ca_temp->ca_io_done_sem);
#			endif
#		endif
		free((char *)ca_temp);

		/*
		 * Only remove task variable if user is calling this from
		 * ca_task_exit and the task is not being deleted 
		 * 
		 * in this case the dest tid will match the current task id
		 *
		 * This is because if I delete the task variable from a vxWorks
		 * exit handler it botches vxWorks task exit
		 */
#		ifdef vxWorks
			if (tid == taskIdSelf()) {
				int             status;

				ca_static = (struct ca_static *) NULL;

				status = taskVarDelete(tid, &ca_static);
				if (status == ERROR)
					ca_signal(
						ECA_INTERNAL, 
						"Unable to remove ca_static from task var list");
			}
#		endif

	}
}




/*
 *
 *	CA_BUILD_AND_CONNECT
 *
 *
 */
ca_build_and_connect
#ifdef VAXC
(
 char *name_str,
 chtype get_type,
 unsigned int get_count,
 chid * chixptr,
 void *pvalue,
 void (*conn_func) (),
 void *puser
)
#else
(name_str, get_type, get_count, chixptr, pvalue, conn_func, puser)
	char           *name_str;
	chtype          get_type;
	unsigned int    get_count;
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

			/*
			 * allocate CHIU (channel in use) block
			 *
			 * also allocate enough for the channel name & paddr
			 * block
			 */
			*chixptr = chix = (chid) malloc(sizeof(*chix)
					 + strcnt + sizeof(struct db_addr));
			if (!chix)
				abort();

			chix->paddr = (void *) (strcnt + (char *) (chix + 1));
			*(struct db_addr *) chix->paddr = tmp_paddr;
			chix->puser = puser;
			chix->connection_func = conn_func;
			chix->type = ((struct db_addr *) 
					chix->paddr)->field_type;
			chix->count = ((struct db_addr *) 
					chix->paddr)->no_elements;
			chix->iocix = LOCAL_IIU;
			chix->state = cs_conn;
			lstInit(&chix->eventq);
			strncpy(chix + 1, name_str, strcnt);

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
			lstAdd(&local_chidlist, chix);
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

	if (ca_static->ca_cast_available) {
		/* allocate CHIU (channel in use) block */
		/* also allocate enough for the channel name */
		*chixptr = chix = (chid) malloc(sizeof(*chix) + strcnt);
		if (!chix)
			return ECA_ALLOCMEM;

		LOCK;
		status = alloc_ioc(
				   &ca_static->ca_castaddr,
				   IPPROTO_UDP,
				   &chix->iocix
			);
		if (~status & CA_M_SUCCESS) {
			*chixptr = (chid) NULL;
			free((char *) chix);
			goto exit;
		}
		chix->puser = puser;
		chix->connection_func = conn_func;
		chix->type = TYPENOTCONN;	/* invalid initial type 	 */
		chix->count = 0;/* invalid initial count	 */
		chix->paddr = (void *) NULL;	/* invalid initial paddr	 */

		/* save stuff for build retry if required */
		chix->build_type = get_type;
		chix->build_count = get_count;
		chix->build_value = (void *) pvalue;
		chix->name_length = strcnt;
		chix->state = cs_never_conn;
		lstInit(&chix->eventq);

		/* Save this channels name for retry if required */
		strncpy(chix + 1, name_str, strcnt);

		lstAdd(&iiu[BROADCAST_IIU].chidlist, chix);
		/*
		 * set the conn tries back to zero so this channel's location
		 * can be found
		 */
		iiu[BROADCAST_IIU].nconn_tries = 0;
		iiu[BROADCAST_IIU].next_retry = CA_CURRENT_TIME;
		iiu[BROADCAST_IIU].retry_delay = CA_RECAST_DELAY;

		build_msg(chix, DONTREPLY);
		if (!chix->connection_func) {
			SETPENDRECV;
			if (VALID_BUILD(chix))
				SETPENDRECV;
		}
exit:

		UNLOCK;
	}

	return status;

}



/*
 * BUILD_MSG()
 * 
 * NOTE:	*** lock must be applied while in this routine ***
 * 
 */
void build_msg(chix, reply_type)
	chid            chix;
	int             reply_type;
{
	register int    	size;
	register int    	cmd;
	register struct extmsg	*mptr;
	struct ioc_in_use	*piiu;

	piiu = &iiu[chix->iocix];

	if (VALID_BUILD(chix)) {
		size = chix->name_length + sizeof(struct extmsg);
		cmd = IOC_BUILD;
	} else {
		size = chix->name_length;
		cmd = IOC_SEARCH;
	}

	mptr = CAC_ALLOC_MSG(piiu, size);

	mptr->m_cmmd = htons(cmd);
	mptr->m_available = (int) chix;
	mptr->m_type = reply_type;
	mptr->m_count = 0;
	mptr->m_pciu = (void *) chix;

	if (cmd == IOC_BUILD) {
		/* msg header only on db read req	 */
		mptr++;
		mptr->m_postsize = 0;
		mptr->m_cmmd = htons(IOC_READ_BUILD);
		mptr->m_type = htons(chix->build_type);
		mptr->m_count = htons(chix->build_count);
		mptr->m_available = (int) chix->build_value;
		mptr->m_pciu = 0;
	}
	/* 
	 * channel name string - forces a NULL at the end because 
	 * strcnt is allways >= strlen + 1 
	 */
	mptr++;
	strncpy(mptr, chix + 1, chix->name_length);

	CAC_ADD_MSG(piiu);

	piiu->send_needed = TRUE;
}



/*
 * CA_ARRAY_GET()
 * 
 * 
 */
ca_array_get
#ifdef VAXC
(
 chtype type,
 unsigned int count,
 chid chix,
 register void *pvalue
)
#else
(type, count, chix, pvalue)
	chtype          type;
	unsigned int    count;
	chid            chix;
	register void  *pvalue;
#endif
{
	register struct extmsg	*mptr;
	unsigned		size=0;

	CHIXCHK(chix);

	if (INVALID_DB_REQ(type))
		return ECA_BADTYPE;

	if (count > chix->count)
		return ECA_BADCOUNT;

#ifdef vxWorks
	{
		int             status;

		if (chix->iocix == LOCAL_IIU) {
			status = db_get_field(
					      chix->paddr,
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

	LOCK;
	mptr = CAC_ALLOC_MSG(&iiu[chix->iocix], size);

	/* 
	 * 	msg header only on db read req	 
	 */
	mptr->m_cmmd = htons(IOC_READ);
	mptr->m_type = htons(type);
	mptr->m_available = (long) pvalue;
	mptr->m_count = htons(count);
	mptr->m_pciu = chix->paddr;

	CAC_ADD_MSG(&iiu[chix->iocix]);
	UNLOCK;

	SETPENDRECV;

	return ECA_NORMAL;
}



/*
 * CA_ARRAY_GET_CALLBACK()
 * 
 * 
 * 
 */
ca_array_get_callback
#ifdef VAXC
(
 chtype type,
 unsigned int count,
 chid chix,
 void (*pfunc) (),
 void *arg
)
#else
(type, count, chix, pfunc, arg)
	chtype          type;
	unsigned int    count;
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
	if (chix->iocix == LOCAL_IIU) {
		struct pending_event ev;

		ev.usr_func = pfunc;
		ev.usr_arg = arg;
		ev.chan = chix;
		ev.type = type;
		ev.count = count;
		ca_event_handler(&ev, chix->paddr, NULL, NULL);
		return ECA_NORMAL;
	}
#endif

	LOCK;
	if (!(monix = (evid) lstGet(&free_event_list)))
		monix = (evid) malloc(sizeof *monix);

	if (monix) {

		monix->chan = chix;
		monix->usr_func = pfunc;
		monix->usr_arg = arg;
		monix->type = type;
		monix->count = count;

		lstAdd(&pend_read_list, monix);

		issue_get_callback(monix);

		status = ECA_NORMAL;
	} else
		status = ECA_ALLOCMEM;

	UNLOCK;
	return status;
}



/*
 *
 *	ISSUE_GET_CALLBACK()
 *	(lock must be on)
 */
static void 
issue_get_callback(monix)
	register evid   monix;
{
	register chid   	chix = monix->chan;
	unsigned		size = 0;
	unsigned        	count;
	register struct extmsg	*mptr;
	struct ioc_in_use	*piiu;

	piiu = &iiu[chix->iocix];

  	/* 
	 * dont send the message if the conn is down 
	 * (it will be sent once connected)
  	 */
	if(chix->state != cs_conn){
		return;
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
	
	mptr = CAC_ALLOC_MSG(piiu, size);

	/* msg header only on db read notify req	 */
	mptr->m_cmmd = htons(IOC_READ_NOTIFY);
	mptr->m_type = htons(monix->type);
	mptr->m_available = (long) monix;
	mptr->m_count = htons(count);
	mptr->m_pciu = chix->paddr;

	CAC_ADD_MSG(piiu);

	piiu->send_needed = TRUE;
}


/*
 *	CA_ARRAY_PUT()
 *
 *
 */
ca_array_put
#ifdef VAXC
(
chtype				type,
unsigned int			count,
chid				chix,
void				*pvalue
)
#else
(type,count,chix,pvalue)
register chtype			type;
unsigned int			count;
chid				chix;
register void 			*pvalue;
#endif
{
	register struct extmsg	*mptr;
  	register int  		postcnt;
  	register void		*pdest;
  	register unsigned	size_of_one;
  	register int 		i;

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
  	{
    		int status;

    		if(chix->iocix == LOCAL_IIU){
      			status = db_put_field(  chix->paddr,  
                                		type,   
                                		pvalue,
                                		count);            
      			if(status==OK)
				return ECA_NORMAL;
      			else
        			return ECA_PUTFAIL;
    		}
  	}
#endif

  	size_of_one = dbr_size[type];
  	postcnt = size_of_one*count;

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
	mptr = CAC_ALLOC_MSG(&iiu[chix->iocix], postcnt);
    	pdest = (void *)(mptr+1);


#ifdef VAX
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
#endif
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
        		return ECA_BADTYPE;
      		}
      		(char *) pdest += size_of_one;
      		(char *) pvalue += size_of_one;
    	}
#else
        memcpy(pdest,pvalue,postcnt);
#endif

    	mptr->m_cmmd 		= htons(IOC_WRITE);
   	mptr->m_type		= htons(type);
    	mptr->m_count 		= htons(count);
    	mptr->m_pciu 		= chix->paddr;
    	mptr->m_available 	= (long) chix;

	CAC_ADD_MSG(&iiu[chix->iocix]);
	UNLOCK;

  	return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for connection events
 *
 */
ca_change_connection_event
#ifdef VAXC
(
chid		chix,
void 		(*pfunc)()
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
ca_add_exception_event
#ifdef VAXC
(
void 		(*pfunc)(),
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
ca_add_io_event
#ifdef VAXC
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
    LOCK;
    pioe = (struct pending_io_event *) malloc(sizeof(*pioe));
    if(!pioe)
      return ECA_ALLOCMEM;
    pioe->io_done_arg = astarg;
    pioe->io_done_sub = ast;
    lstAdd(&ioeventlist,pioe);
    UNLOCK;
  }

  return ECA_NORMAL;
}




/*
 *	CA_ADD_MASKED_ARRAY_EVENT
 *
 *
 */
ca_add_masked_array_event
#ifdef VAXC
(
chtype 		type,
unsigned int	count,
chid 		chix,
void 		(*ast)(),
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
unsigned int			count;
chid				chix;
void				(*ast)();
void				*astarg;
ca_real				p_delta;
ca_real				n_delta;
ca_real				timeout;
evid				*monixptr;
unsigned			mask;
#endif
{
  register evid			monix;
  register int			status;

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

    if(chix->iocix == LOCAL_IIU){

      if(!dbevsize)
        dbevsize = db_sizeof_event_block();


      if(!(monix = (evid)lstGet(&dbfree_ev_list)))
        monix = (evid)malloc(sizeof(*monix)+dbevsize);
    }
    else
      if(!(monix = (evid)lstGet(&free_event_list)))
        monix = (evid)malloc(sizeof *monix);
  }
# else
  if(!(monix = (evid)lstGet(&free_event_list)))
    monix = (evid) malloc(sizeof *monix);
# endif

  if(!monix){
    status = ECA_ALLOCMEM;
    goto unlock_rtn;
  }

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
    struct monops 	mon;

    if(chix->iocix == LOCAL_IIU){
      status = db_add_event(	evuser,
				chix->paddr,
				ca_event_handler,
				monix,
				mask,
				monix+1);
      if(status == ERROR){
        status = ECA_DBLCLFAIL;
        goto unlock_rtn;
      }

      /* 
	Place in the channel list 
	- do it after db_add_event so there
	is no chance that it will be deleted 
	at exit before it is completely created
      */
      lstAdd(&chix->eventq, monix);

      /* 
	force event to be called at least once
      	return warning msg if they have made the queue to full 
	to force the first (untriggered) event.
      */
      if(db_post_single_event(monix+1)==ERROR){
        status = ECA_OVEVFAIL;
        goto unlock_rtn;
      }

      status = ECA_NORMAL;
      goto unlock_rtn;

    } 
  }
# endif

  /* It can be added to the list any place if it is remote */
  /* Place in the channel list */
  lstAdd(&chix->eventq, monix);

  ca_request_event(monix);

  status = ECA_NORMAL;

unlock_rtn:

  UNLOCK;

  return status;
}


/*
 *	CA_REQUEST_EVENT()
 *
 * 	LOCK must be applied while in this routine
 */
void 
ca_request_event(monix)
	evid            monix;
{
	register chid   	chix = monix->chan;
	unsigned		size = sizeof(struct mon_info);
	unsigned        	count;
	register struct monops	*mptr;
	struct ioc_in_use	*piiu;

	piiu = &iiu[chix->iocix];

  	/* 
	 * dont send the message if the conn is down 
	 * (it will be sent once connected)
  	 */
	if(chix->state != cs_conn){
		return;
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

	mptr = (struct monops *) CAC_ALLOC_MSG(piiu, size);

	/* msg header	 */
	mptr->m_header.m_cmmd = htons(IOC_EVENT_ADD);
	mptr->m_header.m_available = (long) monix;
	mptr->m_header.m_type = htons(monix->type);
	mptr->m_header.m_count = htons(count);
	mptr->m_header.m_pciu = chix->paddr;

	/* msg body	 */
	htonf(&monix->p_delta, &mptr->m_info.m_hval);
	htonf(&monix->n_delta, &mptr->m_info.m_lval);
	htonf(&monix->timeout, &mptr->m_info.m_toval);
	mptr->m_info.m_mask = htons(monix->mask);

	CAC_ADD_MSG(piiu);

	piiu->send_needed = TRUE;
}


/*
 *
 *	CA_EVENT_HANDLER()
 *	(only for local (for now vxWorks) clients)
 *
 */
#ifdef vxWorks
static void 
ca_event_handler(monix, paddr, hold, pfl)
register evid 		monix;
register struct db_addr	*paddr;
int			hold;
void			*pfl;
{
  	register int 		status;
  	register int		count;
  	register chtype		type = monix->type;
  	union db_access_val	valbuf;
  	void			*pval;
  	register unsigned	size;
	struct tmp_buff{
		NODE		node;
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
    		if(count == 1)
      			size = dbr_size[type];
    		else
      			size = (count-1)*dbr_value_size[type]+dbr_size[type];

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
				lstDelete(
					&lcl_buff_list,
					pbuf);
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
				if(!pbuf)
					abort();
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
  	 *   I would like to tell um with the event handler but this would
  	 *   not be upward compatible. so I run the exception handler.
   	 */
	LOCKEVENTS;
  	if(status == ERROR){
    		if(ca_static->ca_exception_func){
			struct exception_handler_args	args;

			args.usr = ca_static->ca_exception_arg;	
			args.chid = monix->chan;	
			args.type = type;		
			args.count = count;		
			args.addr = NULL;		
			args.stat = ECA_GETFAIL;	
			args.op = CA_OP_GET;		
			args.ctx = "Event lost due to local get fail\n"; 

        		(*ca_static->ca_exception_func)(args);
    		}
  	}
  	else{
   	 	(*monix->usr_func)(
				monix->usr_arg, 
				monix->chan, 
				type, 
				count,
				pval);
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

		lstInsert(	
			&lcl_buff_list,
			ptbuf,
			pbuf);
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
ca_clear_event
#ifdef VAXC
(register evid monix)
#else
(monix)
	register evid   monix;
#endif
{
	register chid   chix = monix->chan;
	register struct extmsg *mptr;

	LOOSECHIXCHK(chix);

	/* disable any further events from this event block */
	monix->usr_func = NULL;

#ifdef vxWorks
	if (chix->iocix == LOCAL_IIU) {
		register int    status;

		/*
		 * dont allow two threads to delete the same moniitor at once
		 */
		LOCK;
		status = lstFind(&chix->eventq, monix);
		if (status != ERROR) {
			lstDelete(&chix->eventq, monix);
			status = db_cancel_event(monix + 1);
		}
		UNLOCK;
		if (status == ERROR)
			return ECA_BADMONID;

		lstAdd(&dbfree_ev_list, monix);

		return ECA_NORMAL;
	}
#endif

	/*
	 * dont send the message if the conn is down (just delete from the
	 * queue and return)
	 * 
	 * check for conn down while locked to avoid a race
	 */
	LOCK;
	if(chix->state == cs_conn){
		mptr = CAC_ALLOC_MSG(&iiu[chix->iocix], 0);

		/* msg header	 */
		mptr->m_cmmd = htons(IOC_EVENT_CANCEL);
		mptr->m_available = (long) monix;
		mptr->m_type = chix->type;
		mptr->m_count = chix->count;
		mptr->m_pciu = chix->paddr;
	
		/* 
		 *	NOTE: I free the monitor block only
		 *	after confirmation from IOC 
		 */
		CAC_ADD_MSG(&iiu[chix->iocix]);
	}
	else{
		lstDelete(&monix->chan->eventq, monix);
	}
	UNLOCK;

	return ECA_NORMAL;
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
ca_clear_channel
#ifdef VAXC
(register chid chix)
#else
(chix)
	register chid   chix;
#endif
{
	register evid   		monix;
	struct ioc_in_use 		*piiu = &iiu[chix->iocix];
	register struct extmsg 		*mptr;
	enum channel_state		old_chan_state;

	LOOSECHIXCHK(chix);

	/* disable their further use of deallocated channel */
	chix->type = TYPENOTINUSE;
	old_chan_state = chix->state;
	chix->state = cs_closed;

	/* the while is only so I can break to the lock exit */
	LOCK;
	while (TRUE) {
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

#ifdef vxWorks
		if (chix->iocix == LOCAL_IIU) {
			int             status;

			/*
			 * clear out the events for this channel
			 */
			while (monix = (evid) lstGet(&chix->eventq)) {
				status = db_cancel_event(monix + 1);
				if (status == ERROR)
					abort();
				lstAdd(&dbfree_ev_list, monix);
			}

			/*
			 * clear out this channel
			 */
			lstDelete(&local_chidlist, chix);
			free((char *) chix);

			break;	/* to unlock exit */
		}
#endif

		/*
		 * dont send the message if not conn 
		 * (just delete from the queue and return)
		 * 
		 * check for conn state while locked to avoid a race
		 */
		if(old_chan_state != cs_conn){
			lstConcat(&free_event_list, &chix->eventq);
			lstDelete(&piiu->chidlist, chix);
			if (chix->iocix != BROADCAST_IIU && 
					!piiu->chidlist.count){
				close_ioc(piiu);
			}
			free((char *) chix);
			break;	/* to unlock exit */
		}

		/*
		 * clear events and all other resources for this chid on the
		 * IOC
		 */
		mptr = CAC_ALLOC_MSG(&iiu[chix->iocix], 0);

		/* msg header	 */
		mptr->m_cmmd = htons(IOC_CLEAR_CHANNEL);
		mptr->m_available = (int) chix;
		mptr->m_type = 0;
		mptr->m_count = 0;
		mptr->m_pciu = chix->paddr;

		/*
		 * NOTE: I free the chid and monitor blocks only after
		 * confirmation from IOC
		 */

		CAC_ADD_MSG(&iiu[chix->iocix]);

		break;		/* to unlock exit */
	}
	/*
	 * message about unexecuted code from SUN's cheaper cc is a compiler
	 * bug
	 */
	UNLOCK;

	return ECA_NORMAL;
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
/*	function specified with add_event when one occurs. If the 	*/
/*	timeout is specified as 0 infinite timeout is assumed.		*/
/*	if the argument early is specified TRUE then CA_NORMAL is 	*/
/*	returned early (prior to timeout experation) when outstanding 	*/
/*	IO completes.							*/
/*	ca_flush_io() is called by this routine.			*/
/************************************************************************/
#ifdef VAXC
ca_pend(ca_real timeout, int early)
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

  	/*	Flush the send buffers	*/
  	LOCK;
  	cac_send_msg();
 	UNLOCK;

    	if(pndrecvcnt<1 && early){
        	return ECA_NORMAL;
	}

	/*
	 * quick exit if a poll
	 */
  	if((timeout*SYSFREQ)<LOCALTICKS && timeout != 0.0){

		/*
		 * on UNIX call recv_msg() with zero timeout. on vxWorks 
		 * and VMS recv_msg() need not be called.
		 * 
		 * cac_send_msg() only calls recv_msg on UNIX if the 
		 * buffer needs to be flushed.
		 *
		 */
#ifdef UNIX
		{
    			struct timeval	notimeout;
			/*
			 * locking only a comment on UNIX
			 */
  	  		notimeout.tv_sec = 0;
  	 		notimeout.tv_usec = 0;
   			recv_msg_select(&notimeout);
		}
#endif
  		LOCK;
      		manage_conn(!early);
		if(pndrecvcnt>0 && early){
			ca_pend_io_cleanup();
		}
  		UNLOCK;

    		if(pndrecvcnt<1 && early){
        		return ECA_NORMAL;
		}
		else{
      	 		return ECA_TIMEOUT;
		}
  	}

  	beg_time = time(NULL);

  	while(TRUE){
#		if defined(UNIX)
    		{
      			struct timeval	itimeout;

      			itimeout.tv_usec 	= LOCALTICKS;	
      			itimeout.tv_sec  	= 0;
      			LOCK;
      			recv_msg_select(&itimeout);
      			UNLOCK;
    		}
#		elif defined(vxWorks)
#ifdef	V5_vxWorks
			semTake(io_done_sem, LOCALTICKS);
#else
			{
				int dummy;
				vrtxPend(&io_done_sem->count, LOCALTICKS, &dummy);
			}
#endif
#		elif defined(VMS)
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
#		else
			@@@@ dont compile in this case @@@@ 
#		endif

   		LOCK;
      		manage_conn(TRUE);
    		UNLOCK;

    		if(pndrecvcnt<1 && early)
        		return ECA_NORMAL;

    		if(timeout != 0.0){
      			if(timeout < time(NULL)-beg_time){
				LOCK;
      				manage_conn(!early);
				if(early){
					ca_pend_io_cleanup();
				}
				UNLOCK;
        			return ECA_TIMEOUT;
      			}
  		}    
	}
}


/*
 *
 * set pending IO count back to zero and
 * send a sync to each IOC and back. dont
 * count reads until we recv the sync
 *
 */
static	void 
ca_pend_io_cleanup()
{
	struct ioc_in_use       *piiu;

	for(    piiu = &iiu[BROADCAST_IIU+1];
		piiu<&iiu[nxtiiu];
		piiu++){

		if(piiu->conn_up){
			struct extmsg *mptr;

			piiu->cur_read_seq++;
			mptr = CAC_ALLOC_MSG(piiu, 0);
			*mptr = nullmsg;
			mptr->m_cmmd = htons(IOC_READ_SYNC);
			CAC_ADD_MSG(piiu);
		}
	}
	pndrecvcnt = 0;
}




/*
 *	CA_FLUSH_IO()
 *
 *
 */
ca_flush_io
#ifdef VAXC
(void)
#else
()
#endif
{

  INITCHK;

  /*	Flush the send buffers			*/
  LOCK;
  cac_send_msg();
  UNLOCK;

  return ECA_NORMAL;
}



/*
 *	CA_SIGNAL()
 *
 *
 */
void ca_signal(ca_status,message)
int		ca_status;
char		*message;
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
#   ifdef vxWorks
      ti(VXTHISTASKID);
      tt(VXTHISTASKID);
#   endif
    abort();
  }

  ca_printf(
"..................................................................\n");


}





/*
 *	CA_BUSY_MSG()
 *
 */
void 
ca_busy_message(piiu)
	register struct ioc_in_use *piiu;
{
	struct extmsg  *mptr;

  	/* 
	 * dont send the message if the conn is down 
  	 */
    	if(!piiu->conn_up || piiu == &iiu[BROADCAST_IIU])
		return;

	LOCK;
	mptr = CAC_ALLOC_MSG(piiu, 0);
	*mptr = nullmsg;
	mptr->m_cmmd = htons(IOC_EVENTS_OFF);
	CAC_ADD_MSG(piiu);
	piiu->send_needed = TRUE;
	UNLOCK;
}


/*
 * CA_READY_MSG()
 * 
 */
void 
ca_ready_message(piiu)
	register struct ioc_in_use *piiu;
{
	struct extmsg  *mptr;

  	/* 
	 * dont send the message if the conn is down 
  	 */
    	if(!piiu->conn_up || piiu == &iiu[BROADCAST_IIU])
		return;

	LOCK;
	mptr = CAC_ALLOC_MSG(piiu, 0);
	*mptr = nullmsg;
	mptr->m_cmmd = htons(IOC_EVENTS_ON);
	CAC_ADD_MSG(piiu);
	piiu->send_needed = TRUE;
	UNLOCK;

}


/*
 * NOOP_MSG (lock must be on)
 * 
 */
void
noop_msg(piiu)
	register struct ioc_in_use *piiu;
{
	struct extmsg  *mptr;

	mptr = CAC_ALLOC_MSG(piiu, 0);
	*mptr = nullmsg;
	mptr->m_cmmd = htons(IOC_NOOP);
	CAC_ADD_MSG(piiu);
	piiu->send_needed = TRUE;
}


/*
 * ISSUE_CLAIM_CHANNEL (lock must be on)
 * 
 */
void
issue_claim_channel(piiu, pchan)
struct ioc_in_use	*piiu;
chid 			pchan;
{
	struct extmsg  *mptr;

  	/* 
	 * dont send the message if the conn is down 
  	 */
    	if(!piiu->conn_up || piiu == &iiu[BROADCAST_IIU])
		return;

	mptr = CAC_ALLOC_MSG(piiu, 0);
	*mptr = nullmsg;
	mptr->m_cmmd = htons(IOC_CLAIM_CIU);
	mptr->m_pciu = pchan->paddr;
	CAC_ADD_MSG(piiu);
	piiu->send_needed = TRUE;
}



/*
 *
 *	Default Exception Handler
 *
 *
 */
void
ca_default_exception_handler(args)
struct exception_handler_args args;
{
 
      /* 
	NOTE:
	I should print additional diagnostic info when time permits......
      */

      ca_signal(args.stat, args.ctx);
}



/*
 *	CA_ADD_FD_REGISTRATION
 *
 *	call their function with their argument whenever a new fd is added or removed
 *	(for a manager of the select system call under UNIX)
 *
 */
ca_add_fd_registration(func, arg)
void	(*func)();
void	*arg;
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
ca_defunct()
{
#ifdef VMS
	SEVCHK(ECA_DEFUNCT, "you must have a VMS share image mismatch\n");
#else
	SEVCHK(ECA_DEFUNCT, NULL);
#endif
}


/*
 *	CA_HOST_NAME_FUNCTION()	
 *
 * 	returns a pointer to the channel's host name
 *
 *	currently implemented as a function 
 *	(may be implemented as a MACRO in the future)
 */
char
*ca_host_name_function(chix)
chid	chix;
{
	return iiu[chix->iocix].host_name_str;
}
