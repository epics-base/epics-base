/*
 *	$Id$	
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#include "callback.h"
#include "iocinf.h"
#include "remLib.h"
#include "dbEvent.h"
#include "freeList.h"

LOCAL void ca_repeater_task();
LOCAL void ca_task_exit_tcb(WIND_TCB *ptcb);
LOCAL void ca_extra_event_labor(void *pArg);
LOCAL int cac_os_depen_exit_tid (struct CA_STATIC *pcas, int tid);
LOCAL int cac_add_task_variable (struct CA_STATIC *ca_temp);
LOCAL void deleteCallBack(CALLBACK *pcb);
LOCAL void ca_check_for_fp();
LOCAL int event_import(int tid);

/*
 * order of ops is important here
 *
 * NOTE: large OS dependent SYFREQ might cause an overflow
 * NOTE: POLLDELAY must be less than TICKSPERSEC
 */
#define POLLDELAY	50             /* milli sec 		*/
#define TICKSPERSEC     1000           /* milli sec per sec    */
#define LOCALTICKS      ((sysClkRateGet()*POLLDELAY)/TICKSPERSEC)

#ifdef DEBUG
#define ifDepenDebugPrintf(argsInParen) printf argsInParen
#else
#define ifDepenDebugPrintf(argsInParen)
#endif

/*
 * cac_gettimeval()
 */
void cac_gettimeval(struct timeval  *pt)
{
	unsigned long		sec;
	unsigned long		current;
	static unsigned long	rate;
	static unsigned long	last;
	static unsigned long	offset;
	static SEM_ID		sem;
	int			status;

	assert(pt);

	/*
 	 * Lazy Init
	 */
	if(!rate){
		sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
		rate = sysClkRateGet();
		assert(rate);
		assert(sem!=NULL);
	}
	else {
		status = semTake(sem, WAIT_FOREVER);
		assert(status==OK);
	}

	current = tickGet();
	if(current<last){
		offset += (~0UL)/rate;
	}
	last = current;
	status = semGive(sem);
	assert(status==OK);
	
	sec = current/rate;
        pt->tv_sec = sec + offset;
        pt->tv_usec = ((current-sec*rate)*USEC_PER_SEC)/rate;
}

/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	struct timeval  itimeout;
	int		ticks;
	int		rate = sysClkRateGet();

#ifdef NOASYNCRECV 
	cac_mux_io(pTV, TRUE);
#else
	/*
	 * flush outputs
	 * (recv occurs in another thread)
	 */
	itimeout.tv_usec = 0;
	itimeout.tv_sec = 0;
	cac_mux_io (&itimeout, TRUE);
        
	ticks = (int) (pTV->tv_sec*rate + (pTV->tv_usec*rate)/USEC_PER_SEC);
	ticks = min(LOCALTICKS, ticks);

	semTake (io_done_sem, ticks);
	/*
	 * force a time update because we are not
	 * going to get one with a nill timeout in
	 * ca_mux_io()
	 */
	cac_gettimeval (&ca_static->currentTime);
#endif
}

/*
 * os_specific_sg_create()
 */
void os_specific_sg_create(CASG   *pcasg)
{
	pcasg->sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	assert (pcasg->sem);
}

/*
 * os_specific_sg_delete()
 */
void os_specific_sg_delete(CASG   *pcasg)
{
	int status;

	status = semDelete(pcasg->sem);
	assert (status == OK);
}

/*
 * os_specific_sg_io_complete()
 */
void os_specific_sg_io_complete(CASG   *pcasg)
{
	int status;

	status = semGive(pcasg->sem);
	assert (status == OK);
}

/*
 * cac_block_for_sg_completion()
 */
void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
    struct timeval  itimeout;
    int             ticks;
    int             rate = sysClkRateGet();
    
#ifdef NOASYNCRECV 
    cac_mux_io(pTV, TRUE);
#else
    /*
     * flush outputs
     * (recv occurs in another thread)
     */
    itimeout.tv_usec = 0;
    itimeout.tv_sec = 0;
    cac_mux_io(&itimeout, TRUE);
    
    ticks = (int) (pTV->tv_sec*rate + (pTV->tv_usec*rate)/USEC_PER_SEC);
    ticks = min(LOCALTICKS, ticks);
    
    semTake (pcasg->sem, ticks);
    /*
     * force a time update because we are not
     * going to get one with a nill timeout in
     * ca_mux_io()
     */
    cac_gettimeval (&ca_static->currentTime);
#endif
}

/*
 * CAC_ADD_TASK_VARIABLE()
 */
LOCAL int cac_add_task_variable (struct CA_STATIC *ca_temp)
{
        static char             ca_installed;
        TVIU                    *ptviu;
        int                     status;

        ca_check_for_fp();
	
#       ifdef DEBUG
                ca_printf("CAC: adding task variable\n");
#       endif

        status = taskVarGet(VXTHISTASKID, (int *)&ca_static);
        if(status == OK){
                ca_printf("task variable already installed?\n");
                return ECA_INTERNAL;
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
#               ifdef DEBUG
                        ca_printf("CAC: adding delete hook\n");
#               endif

                status = taskVarInit();
                if (status != OK)
                        return ECA_INTERNAL;
                status = taskDeleteHookAdd((FUNCPTR)ca_task_exit_tcb);
                if (status != OK) {
                        ca_printf("ca_init_task: could not add CA delete routine\n"
);
                        return ECA_INTERNAL;
                }
        }

        ptviu = (TVIU *) calloc(1, sizeof(*ptviu));
        if(!ptviu){
                return ECA_INTERNAL;
        }

        ptviu->tid = taskIdSelf();

        status = taskVarAdd(VXTHISTASKID, (int *)&ca_static);
        if (status != OK){
                free(ptviu);
                return ECA_INTERNAL;
        }

		/* 
		 * Care is taken not to set the value of the 
		 * ca static task variable until after it has been installed
		 * so that we dont change the value for other tasks.
		 */
        ca_static = ca_temp;
        ellAdd(&ca_temp->ca_taskVarList, &ptviu->node);

        return ECA_NORMAL;
}

/*
 *      CA_TASK_EXIT_TCBX()
 *
 */
LOCAL void ca_task_exit_tcb(WIND_TCB *ptcb)
{
	int			status;
	struct CA_STATIC	*ca_temp;

#       ifdef DEBUG
                ca_printf("CAC: entering the exit handler %x\n", ptcb);
#       endif

        /*
         * NOTE: vxWorks provides no method at this time
         * to get the task id from the ptcb so I am
         * taking the liberty of using the ptcb as
         * the task id - somthing which may not be true
         * on future releases of vxWorks
         */
	ca_temp = (struct CA_STATIC *)
		taskVarGet((int)ptcb, (int *) &ca_static);
	if (ca_temp == (struct CA_STATIC *) ERROR){
		return;
	}

	/*
	 * Add CA task var for the exit handler
	 */
	if (ptcb != taskIdCurrent) {
        	status = taskVarAdd (VXTHISTASKID, (int *)&ca_static);
        	if (status == ERROR){
			ca_printf ("Couldnt add task var to CA exit task\n");
                	return;
		}
        }

	/*
	 * normal CA shut down
	 */
	cac_os_depen_exit_tid (ca_temp, (int) ptcb);

	if (ptcb != taskIdCurrent) {
        	status = taskVarDelete(VXTHISTASKID, (int *)&ca_static);
        	if (status == ERROR){
			ca_printf ("Couldnt remove task var from CA exit task\n");
                	return;
		}
        }
}

/*
 * ca_task_initialize()
 */
int ca_task_initialize ()
{
	struct CA_STATIC *pcas;
	char name[15];
	int status;

	pcas = (struct CA_STATIC *)
			taskVarGet (taskIdSelf(), (int *)&ca_static);
	if (pcas != (struct CA_STATIC *) ERROR) {
		return ECA_NORMAL;
	}

	pcas = (struct CA_STATIC *) 
		calloc(1, sizeof(*pcas));
	if (!pcas) {
		return ECA_ALLOCMEM;
	}

	ellInit (&pcas->ca_local_chidlist);
	ellInit (&pcas->ca_lcl_buff_list);
	ellInit (&pcas->ca_taskVarList);
	ellInit (&pcas->ca_putNotifyQue);

	freeListInitPvt (&pcas->ca_dbMonixFreeList,
		db_sizeof_event_block ()+sizeof(struct pending_event),256);

	pcas->ca_tid = taskIdSelf ();
	pcas->ca_client_lock = semMCreate (SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
	assert (pcas->ca_client_lock);
	pcas->ca_putNotifyLock = semMCreate (SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
	assert (pcas->ca_putNotifyLock);
	pcas->ca_io_done_sem = semBCreate (SEM_Q_PRIORITY, SEM_EMPTY);
	assert (pcas->ca_io_done_sem);
	pcas->ca_blockSem = semBCreate (SEM_Q_PRIORITY, SEM_EMPTY);
	assert (pcas->ca_blockSem);

	status = cac_add_task_variable (pcas);
	if (status != ECA_NORMAL) {
		freeListCleanup (pcas->ca_dbMonixFreeList);
		semDelete (pcas->ca_client_lock);
		semDelete (pcas->ca_putNotifyLock);
		semDelete (pcas->ca_io_done_sem);
		semDelete (pcas->ca_blockSem);
		return status;
	}

	status = ca_os_independent_init ();
	if (status != ECA_NORMAL){
		ca_import_cancel (taskIdSelf());
		freeListCleanup (pcas->ca_dbMonixFreeList);
		semDelete (pcas->ca_client_lock);
		semDelete (pcas->ca_putNotifyLock);
		semDelete (pcas->ca_io_done_sem);
		semDelete (pcas->ca_blockSem);
		return status;
	}

	evuser = (void *) db_init_events();
	assert(evuser);

	status = db_add_extra_labor_event(
                                        (struct event_user *)evuser,
                                        ca_extra_event_labor,
                                        pcas);
	assert(status==0);
	strcpy(name, "EV ");
	strncat(
		name,
		taskName(VXTHISTASKID),
		sizeof(name) - strlen(name) - 1);
	status = db_start_events(
			(struct event_user *)evuser,
			name,
			event_import,
			taskIdSelf(),
			-1); /* higher priority */
	assert(status == OK);

	return ECA_NORMAL;
}

/*
 * ca_task_exit ()
 */
int epicsShareAPI ca_task_exit (void)
{
	struct CA_STATIC *pcas;

	pcas = (struct CA_STATIC *)
			taskVarGet (taskIdSelf(), (int *)&ca_static);
	if (pcas == (struct CA_STATIC *) ERROR) {
		return ECA_NOCACTX;
	}

	return cac_os_depen_exit_tid (pcas, 0);
}

/*
 * cac_os_depen_exit_tid ()
 */
LOCAL int cac_os_depen_exit_tid (struct CA_STATIC *pcas, int tid)
{
	int		status;
	ciu		chix;
	miu		monix;
	TVIU    	*ptviu;
	CALLBACK	*pcb;


#	ifdef DEBUG
	ca_printf("CAC: entering the exit routine %x %x\n", 
		tid, pcas);
#	endif

	ca_static = pcas;
	LOCK;

	/*
	 * stop the socket recv task
	 * !! only after we get the LOCK here !!
	 */
	if (taskIdVerify (pcas->recv_tid)==OK) {
		taskwdRemove (pcas->recv_tid);
		/*
		 * dont do a task suspend if the exit handler is
		 * running for this task - it botches vxWorks -
		 */
		if (pcas->recv_tid != tid) {
			status = taskSuspend (pcas->recv_tid);
			if (status<0) {
				ca_printf ("taskSuspend() error = %s\n",
					strerror (errno) );
			}
		}
	}

	/*
	 * Cancel all local events
	 * (and put call backs)
	 *
	 */
	chix = (ciu) & pcas->ca_local_chidlist.node;
	while ( (chix = (ciu) chix->node.next) ) {
		while ( (monix = (miu) ellGet(&chix->eventq)) ) {
			/*
			 * temp release lock so that the event task
			 * 	can finish 
			 */
			UNLOCK;
			status = db_cancel_event((struct event_block *)(monix+1));
			LOCK;
			assert(status == OK);
			freeListFree (ca_static->ca_dbMonixFreeList, monix);
		}
		if (chix->ppn) {
			CACLIENTPUTNOTIFY *ppn;

			ppn = chix->ppn;
			if (ppn->busy) {
				dbNotifyCancel (&ppn->dbPutNotify);
			}
			free (ppn);
		}
	}

	/*
	 * set ca_static for access.c
	 * (run this before deleting the task variable)
	 */
	ca_process_exit();

	/*
	 * cancel task vars for other tasks so this
	 * only runs once
	 *
	 * This is done only after all oustanding events
	 * are drained so that the event task still has a CA context
	 *
	 * db_close_events() does not require a CA context.
	 */
	while ( (ptviu = (TVIU *)ellGet(&pcas->ca_taskVarList)) ) {
#		ifdef DEBUG
                	ca_printf("CAC: removing task var %x\n", ptviu->tid);
#		endif

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

	if (taskIdVerify(pcas->recv_tid)==OK) {
		if(pcas->recv_tid != tid){
			pcb = (CALLBACK *) calloc(1,sizeof(*pcb));
			if (pcb) {
				pcb->callback = deleteCallBack;
				pcb->priority = priorityHigh;
				pcb->user = (void *) pcas->recv_tid;
				callbackRequest (pcb);
			}
		}
	}

	/*
	 * All local events must be canceled prior to closing the
	 * local event facility
	 */
	status = db_close_events(pcas->ca_evuser);
	assert(status == OK);

	ellFree(&pcas->ca_lcl_buff_list);

	/*
	 * remove local chid blocks, paddr blocks, waiting ev blocks
	 */
	ellFree(&pcas->ca_local_chidlist);
	freeListCleanup(pcas->ca_dbMonixFreeList);

	/*
	 * remove semaphores here so that ca_process_exit()
	 * can use them.
	 */
	assert(semDelete(pcas->ca_client_lock)==OK);
	assert(semDelete(pcas->ca_putNotifyLock)==OK);
	assert(semDelete(pcas->ca_io_done_sem)==OK);
	assert(semDelete(pcas->ca_blockSem)==OK);

	ca_static = NULL;
	free ((char *)pcas);

	return ECA_NORMAL;
}

/*
 * deleteCallBack()
 */
LOCAL void deleteCallBack(CALLBACK *pcb)
{
	int status;

	status = taskDelete ((int)pcb->user);
	if (status < 0) {
		ca_printf ("CAC: tak delete at exit failed: %s\n",
			strerror(errno));
	}

	free (pcb);
}

/*
 *
 * localUserName() - for vxWorks
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
	char	*pTmp;
	int	length;
	char    pName[MAX_IDENTITY_LEN];

	remCurIdGet(pName, NULL);

	length = strlen(pName)+1;
	pTmp = malloc(length);
	if(!pTmp){
		return NULL;
	}
	strncpy(pTmp, pName, length-1);
	pTmp[length-1] = '\0';

	return pTmp;
}

/*
 * event_import()
 */
LOCAL int event_import(int tid)
{
	int status;

	status = ca_import(tid);
	if (status==ECA_NORMAL) {
		return OK;	
	}
	else {
		return ERROR;
	}
}

/*
 * CA_IMPORT()
 *
 *
 */
int ca_import (int tid)
{
	int status;
	struct CA_STATIC *pcas;
	TVIU *ptviu;

	ca_check_for_fp();

	/*
	 * just return success if they have already done
	 * a ca import for this task
	 */
	pcas = (struct CA_STATIC *)
			taskVarGet (taskIdSelf(), (int *)&ca_static);
	if (pcas != (struct CA_STATIC *) ERROR) {
		return ECA_NORMAL;
	}

	pcas = (struct CA_STATIC *)
			taskVarGet (tid, (int *)&ca_static);
	if (pcas == (struct CA_STATIC *) ERROR) {
		ca_static = NULL;
		return ECA_NOCACTX;
	}

	ptviu = (TVIU *) calloc (1, sizeof(*ptviu));
	if(!ptviu){
		ca_static = NULL;
		return ECA_ALLOCMEM;
	}

	status = taskVarAdd (VXTHISTASKID, (int *)&ca_static);
	if (status == ERROR){
		ca_static = NULL;
		free (ptviu);
		return ECA_ALLOCMEM;
	}

	/* 
	 * Care is taken not to set the value of the 
	 * ca static task variable until after it has been installed
	 * so that we dont change the value for other tasks.
	 */
	ca_static = pcas;

	ptviu->tid = taskIdSelf();
	LOCK;
	ellAdd(&ca_static->ca_taskVarList, &ptviu->node);
	UNLOCK;

	return ECA_NORMAL;
}

/*
 * CA_IMPORT_CANCEL()
 */
int ca_import_cancel(int tid)
{
	int 			status;
	TVIU    		*ptviu;
	struct CA_STATIC 	*pcas;

	if (tid == taskIdSelf()) {
		pcas = NULL;
	}
	else {
		pcas = ca_static;
	}

	/*
	 * Attempt to attach to the specified context
	 */
	ca_static = (struct CA_STATIC *) 
		taskVarGet(tid, (int *)&ca_static);
	if (ca_static == (struct CA_STATIC *) ERROR){
		ca_static = pcas;
		return ECA_NOCACTX;
	}

	LOCK;
	ptviu = (TVIU *) ellFirst(&ca_static->ca_taskVarList);
	while (ptviu) {
		if(ptviu->tid == tid){
				break;
		}
		ptviu = (TVIU *) ellNext(&ptviu->node);
	}

	if(!ptviu){
		ca_static = pcas;
		UNLOCK;
		return ECA_NOCACTX;
	}

	ellDelete(&ca_static->ca_taskVarList, &ptviu->node);
	free(ptviu);
	UNLOCK;

	status = taskVarDelete(tid, (void *)&ca_static);
	assert (status == OK);

	ca_static = pcas;

	return ECA_NORMAL;
}

/*
 * ca_check_for_fp()
 */
LOCAL void ca_check_for_fp()
{
	int             options;

	assert(taskOptionsGet(taskIdSelf(), &options) == OK);
	if (!(options & VX_FP_TASK)) {
		genLocalExcep (ECA_NEEDSFP, taskName(taskIdSelf()));
        }
}

/*
 *      ca_spawn_repeater()
 *
 *      Spawn the repeater task as needed
 */
void ca_spawn_repeater()
{
	int     status;

	status = taskSpawn(
                           CA_REPEATER_NAME,
                           CA_REPEATER_PRI,
                           CA_REPEATER_OPT,
                           CA_REPEATER_STACK,
			   (FUNCPTR)ca_repeater_task,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0);
	if (status==ERROR){
       		SEVCHK(ECA_NOREPEATER, NULL);
        }
}

/*
 * ca_repeater_task()
 */
LOCAL void ca_repeater_task()
{
	taskwdInsert((int)taskIdCurrent, NULL, NULL);
        ca_repeater();
}

/*
 *      CA_EXTRA_EVENT_LABOR
 */
LOCAL void ca_extra_event_labor (void *pArg)
{
	int                     status;
	CACLIENTPUTNOTIFY       *ppnb;
	struct event_handler_args args;
	
	while (TRUE) {
		/*
		 * independent lock used here in order to
		 * avoid any possibility of blocking
		 * the database (or indirectly blocking
		 * one client on another client).
		 */
		semTake (ca_static->ca_putNotifyLock, WAIT_FOREVER);
		ppnb = (CACLIENTPUTNOTIFY *) ellGet (&ca_static->ca_putNotifyQue);
		semGive (ca_static->ca_putNotifyLock);
		
		/*
		 * break to loop exit
		 */
		if (!ppnb) {
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
		if (ppnb->dbPutNotify.status) {
			if (ppnb->dbPutNotify.status == S_db_Blocked) {
				args.status = ECA_PUTCBINPROG;
			}
			else {
				args.status = ECA_PUTFAIL;
			}
		}
		else {
			args.status = ECA_NORMAL;
		}
		
		LOCK;
		(*ppnb->caUserCallback) (args);
		UNLOCK;
		
		ppnb->busy = FALSE;
	}
	
	/*
	 * wakeup the TCP thread if it is waiting for a cb to complete
	 */
	status = semGive (ca_static->ca_blockSem);
	if (status != OK) {
		logMsg ("CA block sem corrupted\n", 0, 0, 0, 0, 0, 0);
	}
}

/*
 * CAC_RECV_TASK()
 *
 */
void cac_recv_task(int  tid)
{
	struct timeval timeout;
	int status;
	int count;
	
	taskwdInsert((int) taskIdCurrent, NULL, NULL);
	
	status = ca_import(tid);
    /*
     * make sure that we stop if ECA_NOCACTX is returned
     */
    if (status!=ECA_NORMAL) {
	    SEVCHK (status, "cac_recv_task()");
        taskSuspend (taskIdSelf());
    }
	
	/*
	 * once started, does not exit until
	 * ca_task_exit() is called.
	 */
	while (TRUE) {
#ifdef NOASYNCRECV 
		taskDelay (60);
#else

		
		/*
		 * first check for pending recv's with a 
		 * zero time out so that
		 * 1) flow control works correctly (and)
		 * 2) we queue up sends resulting from recvs properly
		 */
		while (TRUE) {
			CLR_CA_TIME(&timeout);
			count = cac_select_io (&timeout, CA_DO_RECVS);
			if (count<=0) {
				break;
			}
			ca_process_input_queue ();
		}
		
		manage_conn ();
		
		/*
		 * flush out all pending io prior to blocking
		 *
		 * NOTE: this must be longer than one vxWorks
		 * tick or we will infinite loop 
		 */
		timeout.tv_usec = (4/*ticks*/ * USEC_PER_SEC)/sysClkRateGet();
		timeout.tv_sec = 0;
		count = cac_select_io (&timeout, CA_DO_RECVS|CA_DO_SENDS);

		ca_process_input_queue ();

		checkConnWatchdogs (TRUE);
#endif
	}
}

/*
 * caSetDefaultPrintfHandler()
 *
 * replace the default printf handler with a 
 * vxWorks specific one that calls logMsg ()
 * so that:
 *
 * o messages go to the log file
 * o messages dont get intermixed
 */
void caSetDefaultPrintfHandler ()
{
	ca_static->ca_printf_func = epicsVprintf;
}

/*
 * local_addr()
 * 
 * A valid non-loopback local address is required in the
 * beacon message in certain situations where
 * there are beacon repeaters and there are addresses
 * in the EPICS_CA_ADDRESS_LIST for which we dont have
 * a strictly correct local server address on a multi-interface
 * system. In this situation we use the first valid non-loopback local
 * address found in the beacon message.
 *
 * Dont use ca_static based lock macros here because this is
 * also called by the server. All locks required are applied at
 * a higher level.
 *
 * Starting with TII there are subtle differences between the vxWorks
 * net interface API and most other UNIX implementations
 */
int local_addr (int s, struct sockaddr_in *plcladdr)
{
    int             	        status;
    struct ifconf   	        ifconf;
    struct ifreq    	        ifreq[25];
    struct ifreq    	        *pifreq;
    struct ifreq                *pIfreqListEnd;
    struct ifreq                *pnextifreq;
    unsigned                    size;
    static struct sockaddr_in   addr;
    static char     	        init = FALSE;
    struct sockaddr_in 	        *tmpaddr;
    
    if (init) {
        *plcladdr = addr;
        return OK;
    }
    
    /* 
     *	get the addr of the first interface found 
     *	(report inconsistent interfaces however)
     */
    ifconf.ifc_len = sizeof ifreq;
    ifconf.ifc_req = ifreq;
    status = socket_ioctl(s, SIOCGIFCONF, &ifconf);
    if (status < 0 || ifconf.ifc_len == 0) {
        ca_printf(
            "CAC: SIOCGIFCONF ioctl failed because \"%s\"\n", 
            SOCKERRSTR(SOCKERRNO));
        ifconf.ifc_len = 0;
    }

    pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) ifreq);
    pIfreqListEnd--;

    for (pifreq = ifconf.ifc_req; pifreq<=pIfreqListEnd; pifreq = pnextifreq ) {
        unsigned flags;

        /*
         * compute the size of the current if req
         */
#       if BSD >= 44
            /*
             * vxWorks SENS IP kernel
             */
            size = pifreq->ifr_addr.sa_len + sizeof(pifreq->ifr_name);
            if ( size < sizeof(*pifreq)) {
                size = sizeof(*pifreq); 
            }
#       else
            /*
             * original vxWorks
             */
            size = sizeof(*pifreq);
#       endif

        pnextifreq = (struct ifreq *) (size + (char *) pifreq);
        
        if (pifreq->ifr_addr.sa_family != AF_INET){
            ifDepenDebugPrintf ( ("local_addr: interface %s was not AF_INET\n", pifreq->ifr_name) );
            continue;
        }
        
        status = socket_ioctl(s, SIOCGIFFLAGS, pifreq);
        if (status == ERROR){
            ca_printf("local_addr: net intf flags fetch for %s failed\n", pifreq->ifr_name);
            continue;
        }
        
        /*
         * flags are now stored in a union
         */
        flags = (unsigned) pifreq->ifr_flags;

        if (!(flags & IFF_UP)) {
            ifDepenDebugPrintf ( ("local_addr: net intf %s was down\n", pifreq->ifr_name) );
            continue;
        }
        
        /*
         * dont use the loop back interface
         */
        if (flags & IFF_LOOPBACK) {
            ifDepenDebugPrintf ( ("local_addr: ignoring loopback interface: %s\n", pifreq->ifr_name) );
            continue;
        }
        
        status = socket_ioctl(s, SIOCGIFADDR, pifreq);
        if (status == ERROR){
            ifDepenDebugPrintf ( ("local_addr: could not obtain addr for %s\n", pifreq->ifr_name) );
            continue;
        }
        
        ifDepenDebugPrintf ( ("local_addr: net intf %s found\n", pifreq->ifr_name) );
        
        tmpaddr = (struct sockaddr_in *) &pifreq->ifr_addr;
        
        init = TRUE;
        addr = *tmpaddr;
        break;
    }
    
    if(!init){
        return ERROR;
    }
    
    *plcladdr = addr;
    return OK;
}

/*
 *  caDiscoverInterfaces()
 *
 *	This routine is provided with the address of an ELLLIST, a socket
 * 	a destination port number, and a match address. When the 
 * 	routine returns there will be one additional inet address 
 *	(a caAddrNode) in the list for each inet interface found that 
 *	is up and isnt a loop back interface (match addr is INADDR_ANY)
 *	or it matches the specified interface (match addr isnt INADDR_ANY). 
 *	If the interface supports broadcast then I add its broadcast 
 *	address to the list. If the interface is a point to 
 *	point link then I add the destination address of the point to
 *	point link to the list. In either case I set the port number
 *	in the address node to the port supplied in the argument
 *	list.
 *
 * 	LOCK should be applied here for (pList)
 * 	(this is also called from the server)
 *
 *  Dont use ca_static based lock macros here because this is
 *  also called by the server. All locks required are applied at
 *  higher level.
 *
 *  Starting with TII there are subtle differences between the vxWorks
 *  net interface API and most other UNIX implementations
 */
void epicsShareAPI caDiscoverInterfaces
     (ELLLIST *pList, int socket, unsigned short port, struct in_addr matchAddr)
{
     static const unsigned long      nelem = 100;
     struct sockaddr_in              *pInetAddr;
     caAddrNode                      *pNode;
     int                             status;
     struct ifconf                   ifconf;
     struct ifreq                    *pIfreqList;
     struct ifreq                    *pIfreqListEnd;
     struct ifreq                    *pifreq;
     struct ifreq                    *pnextifreq;
     unsigned                        size;
     
     /*
      * use pool so that we avoid using to much stack space
      * under vxWorks
      *
      * nelem is set to the maximum interfaces 
      * on one machine here
      */
     pIfreqList = (struct ifreq *)calloc(nelem, sizeof(*pifreq));
     if(!pIfreqList){
         return;
     }
     
     ifconf.ifc_len = nelem*sizeof(*pifreq);
     ifconf.ifc_req = pIfreqList;
     status = socket_ioctl(socket, SIOCGIFCONF, &ifconf);
     if (status < 0 || ifconf.ifc_len == 0) {
         free(pIfreqList);
         return;
     }
     
     pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) pIfreqList);
     pIfreqListEnd--;
     
     for (pifreq = pIfreqList; pifreq <= pIfreqListEnd; pifreq = pnextifreq ){
         unsigned flags;

        /*
         * compute the size of the current if req
         */
#       if BSD >= 44
            /*
             * vxWorks SENS IP kernel
             */
            size = pifreq->ifr_addr.sa_len + sizeof(pifreq->ifr_name);
            if ( size < sizeof(*pifreq)) {
                size = sizeof(*pifreq);
            }
#       else
            /*
             * original vxWorks
             */
            size = sizeof(*pifreq);
#       endif

        pnextifreq = (struct ifreq *) (size + (char *) pifreq);

        /*
         * If its not an internet inteface then dont use it 
         */
        if (pifreq->ifr_ifru.ifru_addr.sa_family != AF_INET) {
             ifDepenDebugPrintf ( ("caDiscoverInterfaces: interface %s was not AF_INET\n", pifreq->ifr_name) );
             continue;
        }

        status = socket_ioctl(socket, SIOCGIFFLAGS, pifreq);
        if (status) {
            ca_printf ("caDiscoverInterfaces: net intf flags fetch for %s failed\n", pifreq->ifr_name);
            continue;
        }
        
        /*
         * flags are now stored in a union
         */
        flags = (unsigned) pifreq->ifr_ifru.ifru_flags;

        /*
         * dont bother with interfaces that have been disabled
         */
        if (!(flags & IFF_UP)) {
             ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s was down\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * dont use the loop back interface
         */
        if (flags & IFF_LOOPBACK) {
             ifDepenDebugPrintf ( ("caDiscoverInterfaces: ignoring loopback interface: %s\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * Fetch the local address for this interface
         */
        status = socket_ioctl(socket, SIOCGIFADDR, pifreq);
        if (status) {
             ifDepenDebugPrintf ( ("caDiscoverInterfaces: could not obtain addr for %s\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * verify the address family of the interface that was loaded
         */
        if (pifreq->ifr_ifru.ifru_addr.sa_family != AF_INET) {
             ifDepenDebugPrintf ( ("caDiscoverInterfaces: interface %s was not AF_INET\n", pifreq->ifr_name) );
             continue;
        }

        /*
         * save the interface's IP address
         */
        pInetAddr = (struct sockaddr_in *)&pifreq->ifr_ifru;

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if (matchAddr.s_addr != htonl(INADDR_ANY)) {
             if (pInetAddr->sin_addr.s_addr != matchAddr.s_addr) {
                 ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s didnt match\n", pifreq->ifr_name) );
                 continue;
             }
        }

        /*
         * If this is an interface that supports
         * broadcast fetch the broadcast address.
         *
         * Otherwise if this is a point to point 
         * interface then use the destination address.
         *
         * Otherwise CA will not query through the 
         * interface.
         */
        if (flags & IFF_BROADCAST) {
            status = socket_ioctl(
                        socket, 
                        SIOCGIFBRDADDR, 
                        pifreq);
            if (status) {
                ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s: bcast addr fetch fail\n", pifreq->ifr_name) );
                continue;
            }
        }
        else if(flags & IFF_POINTOPOINT){
            status = socket_ioctl(
                         socket, 
                         SIOCGIFDSTADDR, 
                         pifreq);
            if (status){
                ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s: pt to pt addr fetch fail\n", pifreq->ifr_name) );
                continue;
            }
        }
        else{
            ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s: not pt to pt or bcast\n", pifreq->ifr_name) );
            continue;
        }

        ifDepenDebugPrintf ( ("caDiscoverInterfaces: net intf %s found\n", pifreq->ifr_name) );

        pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
        if(!pNode){
            ca_printf ("caDiscoverInterfaces: malloc failed for net intf %s: \n", pifreq->ifr_name);
            continue;
        }

        pNode->destAddr.in = *pInetAddr;
        pNode->destAddr.in.sin_port = htons(port);

        /*
         * LOCK applied externally
         */
        ellAdd(pList, &pNode->node);
    }

    free(pIfreqList);
}


