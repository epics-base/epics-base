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
 *      Modification Log:
 *      -----------------
 *
 */

#include "iocinf.h"
#include "remLib.h"

LOCAL void ca_repeater_task();
LOCAL void ca_task_exit_tcb(WIND_TCB *ptcb);
LOCAL void ca_extra_event_labor(void *pArg);


/*
 * CAC_ADD_TASK_VARIABLE()
 */
int cac_add_task_variable(struct ca_static *ca_temp)
{
        static char             ca_installed;
        TVIU                    *ptviu;
        int                     status;

#       if DEBUG
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
#               if DEBUG
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

        ptviu = calloc(1, sizeof(*ptviu));
        if(!ptviu){
                return ECA_INTERNAL;
        }

        ptviu->tid = taskIdSelf();
        ellAdd(&ca_temp->ca_taskVarList, &ptviu->node);

        status = taskVarAdd(VXTHISTASKID, (int *)&ca_static);
        if (status != OK){
                free(ptviu);
                return ECA_INTERNAL;
        }

        ca_static = ca_temp;

        return ECA_NORMAL;
}


/*
 *      CA_TASK_EXIT_TCBX()
 *
 */
LOCAL void ca_task_exit_tcb(WIND_TCB *ptcb)
{
#       if DEBUG
                ca_printf("CAC: entering the exit handler %x\n", ptcb);
#       endif

        /*
         * NOTE: vxWorks provides no method at this time
         * to get the task id from the ptcb so I am
         * taking the liberty of using the ptcb as
         * the task id - somthing which may not be true
         * on future releases of vxWorks
         */
        ca_task_exit_tid((int) ptcb);
}


/*
 * cac_os_depen_init()
 */
int cac_os_depen_init(struct ca_static *pcas)
{
	char            name[15];
	int             status;

	ellInit(&pcas->ca_local_chidlist);
	ellInit(&pcas->ca_dbfree_ev_list);
	ellInit(&pcas->ca_lcl_buff_list);
	ellInit(&pcas->ca_taskVarList);
	ellInit(&pcas->ca_putNotifyQue);

	pcas->ca_tid = taskIdSelf();
	pcas->ca_local_ticks = LOCALTICKS;
	pcas->ca_client_lock = semMCreate(SEM_DELETE_SAFE);
	assert(pcas->ca_client_lock);
	pcas->ca_event_lock = semMCreate(SEM_DELETE_SAFE);
	assert(pcas->ca_event_lock);
	pcas->ca_putNotifyLock = semMCreate(SEM_DELETE_SAFE);
	assert(pcas->ca_putNotifyLock);
	pcas->ca_io_done_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	assert(pcas->ca_io_done_sem);
	pcas->ca_blockSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	assert(pcas->ca_blockSem);

	evuser = (void *) db_init_events();
	assert(evuser);

	status = db_add_extra_labor_event(
                                        evuser,
                                        ca_extra_event_labor,
                                        pcas);
	assert(status==0);
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
	assert(status == OK);

        return ECA_NORMAL;
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
 * caHostFromInetAddr()
 */
#ifdef __STDC__
void caHostFromInetAddr(struct in_addr *pnet_addr, char *pBuf, unsigned size)
#else /*__STDC__*/
void caHostFromInetAddr(pnet_addr, pBuf, size)
struct in_addr  *pnet_addr;
char            *pBuf;
unsigned        size;
#endif /*__STDC__*/
{
        char    str[INET_ADDR_LEN];

	inet_ntoa_b(*pnet_addr, str);

        /*
         * force null termination
         */
        strncpy(pBuf, str, size-1);
        pBuf[size-1] = '\0';

        return;
}


/*
 * CA_IMPORT()
 *
 *
 */
#ifdef __STDC__
int ca_import(int tid)
#else
int ca_import(tid)
int             tid;
#endif
{
        int             status;
        struct ca_static *pcas;
        TVIU            *ptviu;

        status = ca_check_for_fp();
        if(status != ECA_NORMAL){
                return status;
        }

	/*
	 * just return success if they have already done
	 * a ca import for this task
	 */
        pcas = (struct ca_static *)
                taskVarGet(taskIdSelf(), (int *)&ca_static);
        if (pcas != (struct ca_static *) ERROR){
                return ECA_NORMAL;
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


/*
 * CA_IMPORT_CANCEL()
 */
#ifdef __STDC__
int ca_import_cancel(int tid)
#else /*__STDC__*/
int ca_import_cancel(tid)
int             tid;
#endif /*__STDC__*/
{
        int     status;
        TVIU    *ptviu;

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


/*
 * ca_check_for_fp()
 */
int ca_check_for_fp()
{
        {
                int             options;

                assert(taskOptionsGet(taskIdSelf(), &options) == OK);
                if (!(options & VX_FP_TASK)) {
                        return ECA_NEEDSFP;
                }
        }
        return ECA_NORMAL;
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


/*
 * ca_repeater_task()
 */
void ca_repeater_task()
{
	taskwdInsert((int)taskIdCurrent, NULL, NULL);
        ca_repeater();
}


/*
 * Setup recv thread
 * (OS dependent)
 */
int cac_setup_recv_thread(IIU *piiu)
{
        return ECA_NORMAL;
}



/*
 *      CA_EXTRA_EVENT_LABOR
 */
LOCAL void ca_extra_event_labor(void *pArg)
{
        int                     status;
        CACLIENTPUTNOTIFY       *ppnb;
        struct ca_static        *pcas;
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

