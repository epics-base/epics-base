
/*
 *
 * vxWorks_depen.c
 * %W% %G%
 *
 * vxWorks dependent routines for the CA server
 *
 *
 */

#include <server.h>
#include <inetLib.h>

VERSIONID(vxWorks_depenc,"%W% %G%")

#define MUTEX_SEM_OPTIONS \
(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE)

LOCAL int event_task(
	struct event_user       *evuser,
	void                    (*init_func)(int arg),
	int                     init_func_arg
);

LOCAL void 	osDepenTimerCB(void *pciu);
LOCAL void 	osDepenSendCB(void *pClient);
int 		caServerTask(struct rsrv *pRsrv);
LOCAL int 	caMaxFreeBlockPoll(void);

size_t 		caMaxFreeBlock;

###### no event flush currently (only needed in multi thread env) ####
status = caeqAddFlushEvent (client->evuser, XXXX, client);
assert (status==OK);
			

/*
 * caGetElapsedTime()
 */
caStatus caGetElapsedTime(caElapsedTimeVal *pElapsed)
{
	ULONG	ticks;
	ULONG	rate;

	ticks = tickGet();
	rate = sysClkRateGet();

	pElapsed->tv_sec = ticks * rate;
	pElapsed->tv_usec = ((ticks % rate)*CAServerUSecInSec)/rate; 

	return S_cas_success;
}


/*
 * caOutOfMemory()
 */
int caOutOfMemory(void)
{
	int		status;
	static int 	pollTaskStarted;

	if(!pollTaskStarted){
		caMaxFreeBlock = memFindMax();
		status = taskSpawn(
				MAXFREEBLOCKTASKNAME,
				MAXFREEBLOCKTASKPRI,
				MAXFREEBLOCKTASKOPT,
				MAXFREEBLOCKTASKSTACK,
				caMaxFreeBlockPoll,
				0,0,0,0,0,0,0,0,0,0);
		if(status>=0){
			pollTaskStarted = TRUE;
		}
	}

	return (caMaxFreeBlock<MAX_BLOCK_THRESHOLD);
}


/*
 * caMaxFreeBlockPoll()
 */
LOCAL int caMaxFreeBlockPoll(void)
{
	while(TRUE){
		caMaxFreeBlock = memFindMax();
		taskDelay(MAXFREEBLOCKPOLL*sysClkRateGet());
	}
}


/*
 * 	casPutCBStartTimer()
 */
caStatus casPutCBStartTimer(
casChanInUse	*pciu,
unsigned 	timeOutSec
)
{
	struct client	*pClient = pciu->client;
	caserver	*pRsrv = pClient->cc.pRsrv;
	struct timeval	tv;

	if(pClient->cc.osSpecific.pPutCBAlarm){
		tv.tv_sec = timeOutSec;
		tv.tv_usec = 0;
		pClient->cc.osSpecific.pPutCBAlarm
			= fdmgr_add_timeout(
				pRsrv->osSpecific.pfdctx,
				&tv,
				osDepenTimerCB,
				pciu);
		if(!pClient->cc.osSpecific.pPutCBAlarm){
			return S_cas_noMemory;
		}
	}

	return S_cas_success;
}


/*
 * osDepenTimerCB()
 */
LOCAL void osDepenTimerCB(void *pParam)
{
	casChanInUse	*pciu = (casChanInUse *)pParam;

	pciu->client->cc.osSpecific.pPutCBAlarm = NULL;
	casPutCBTimer(pciu);
}


/*
 * 	casPutCBCancelTimer()
 */
void casPutCBCancelTimer(struct client *pClient)
{
	caserver	*pRsrv = pClient->cc.pRsrv;
	int		status;

	if(pClient->cc.osSpecific.pPutCBAlarm){
		status = fdmgr_clear_timeout(
				pRsrv->osSpecific.pfdctx,
				pClient->cc.osSpecific.pPutCBAlarm);
		assert(status==0);
		pClient->cc.osSpecific.pPutCBAlarm = NULL;
	}
}


/*
 * casDisableSocketRecvIOCallback()
 */
caStatus casDisableSocketRecvIOCallback(struct client *pClient)
{
	int 	status;

	if(pClient->cc.osSpecific.sockRecvIOCallbackEnabled){
		status = fdmgr_clear_callback(
				pClient->cc.pRsrv->osSpecific.pfdctx,
				pClient->cc.sock,
				fdi_read);
		assert(status==0);
		pClient->cc.osSpecific.sockRecvIOCallbackEnabled = FALSE;
	}
	return S_cas_success;
}


/*
 * casEnableSocketRecvIOCallback()
 */
caStatus casEnableSocketRecvIOCallback(struct client *pClient)
{
	caserver	*pRsrv = pClient->cc.pRsrv;
	int		status;

	if(!pClient->cc.osSpecific.sockRecvIOCallbackEnabled){
		status = fdmgr_add_callback(
				pRsrv->osSpecific.pfdctx,
				pClient->cc.sock,
				fdi_read,
				pClient->cc.pCB,
				pClient);
		if(status){
			return S_cas_noMemory;
		}
		pClient->cc.osSpecific.sockRecvIOCallbackEnabled = TRUE;
	}
	return S_cas_success;
}


/*
 * casWaitForIO ()
 */
caStatus casWaitForIO (struct rsrv *pRsrv, caTime *pDelay)
{
	struct timeval	tv;

	tv.tv_sec = caTime->sec;
	tv.tv_usec = caTime->nsec / NSecPerUsec;
	status = fdmgr_pend_event(pRsrv->osSpecific.pfdctx, &tv);
	if (status) {
		return S_cas_internal;
	}
	return S_cas_success;
}


/*
 * casArmSocketSendIOCallback()
 */
caStatus casArmSocketSendIOCallback(struct client *pClient)
{
	caserver	*pRsrv = pClient->cc.pRsrv;
	int		status;

	if(!pClient->cc.osSpecific.sockSendIOCallbackEnabled){
		status = fdmgr_add_callback(
				pRsrv->osSpecific.pfdctx,
				pClient->cc.sock,
				fdi_write,
				osDepenSendCB,
				pClient);
		if(status){
			return S_cas_noMemory;
		}
		pClient->cc.osSpecific.sockSendIOCallbackEnabled = TRUE;
	}
	return S_cas_success;
}


/*
 * casCancelSocketSendIOCallback()
 */
caStatus casCancelSocketSendIOCallback(struct client *pClient)
{
	caserver	*pRsrv = pClient->cc.pRsrv;
	int		status;

	if(pClient->cc.osSpecific.sockSendIOCallbackEnabled){
		status = fdmgr_clear_callback(
				pRsrv->osSpecific.pfdctx,
				pClient->cc.sock,
				fdi_write);
		assert(status==0);
		pClient->cc.osSpecific.sockSendIOCallbackEnabled = FALSE;
	}
	return S_cas_success;
}


/*
 * osDepenSendCB()
 */
LOCAL void osDepenSendCB(void *pParam)
{
	struct client	*pClient = (struct client *) pParam;

	pClient->cc.osSpecific.sockSendIOCallbackEnabled = FALSE;
	(*pClient->sendHandler)(pClient);
}


/*
 * casOSSpecificDelete()
 */
void casOSSpecificDelete(struct rsrv *pRsrv)
{
	int status;

	if(taskIdVerify(pRsrv->osSpecific.selectTask) == OK){
        	taskwdRemove(taskDelete(pRsrv->osSpecific.selectTask));
		taskDelete(pRsrv->osSpecific.selectTask);
	}
	if(pRsrv->osSpecific.clientQLock){
		status = semDelete(pRsrv->osSpecific.clientQLock);
		assert(status==OK);
	}
	if(pRsrv->osSpecific.freeChanQLock){
		status = semDelete(pRsrv->osSpecific.freeChanQLock);
		assert(status==OK);
	}
	if(pRsrv->osSpecific.freeEventQLock){
		status = semDelete(pRsrv->osSpecific.freeEventQLock);
		assert(status==OK);
	}
	if(pRsrv->osSpecific.pvQLock){
		status = semDelete(pRsrv->osSpecific.pvQLock);
		assert(status==OK);
	}
	if(pRsrv->osSpecific.pfdctx){
		status = fdmgr_delete(pRsrv->osSpecific.pfdctx);
		assert(status==OK);
	}
}


/*
 * casOSSpecificInit()
 */
caStatus casOSSpecificInit(struct rsrv *pRsrv)
{
	pRsrv->osSpecific.clientQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!pRsrv->osSpecific.clientQLock){
		return S_cas_noMemory;
	}
	pRsrv->osSpecific.freeChanQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!pRsrv->osSpecific.freeChanQLock){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}
	pRsrv->osSpecific.freeEventQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!pRsrv->osSpecific.freeEventQLock){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}
	pRsrv->osSpecific.pvQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!pRsrv->osSpecific.pvQLock){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}
	pRsrv->osSpecific.pfdctx = fdmgr_init();
	if(!pRsrv->osSpecific.pfdctx){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}
	
	return S_cas_success;
}


/*
 * casOSSpecificStartServer()
 */
caStatus casOSSpecificStartServer(caserver *pRsrv)
{
	int	status;

	status = fdmgr_add_callback(
			pRsrv->osSpecific.pfdctx,
			pRsrv->server_sock,
			fdi_read,
			rsrv_connect,
			pRsrv);
	if(status < 0){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}

        status = taskSpawn(
                REQ_SRVR_NAME,
                REQ_SRVR_PRI,
                REQ_SRVR_OPT,
                RSP_SRVR_STACK,
                caServerTask,
                (int)pRsrv,
                0,0,0,0,0,0,0,0,0);
        if(status == ERROR){
		casOSSpecificDelete(pRsrv);
		return S_cas_noMemory;
	}
	pRsrv->osSpecific.selectTask = status;


	return S_cas_success;
}


/*
 * caServerTask()
 */
int caServerTask(caserver *pRsrv)
{
	struct timeval	tv;
	caTime		delay;
	struct client	*pClient;
	int		nchars;
	int 		status;

        taskwdInsert((int)taskIdCurrent,NULL,NULL);

	while(TRUE){

		delay.sec = 100;
		delay.usec = 0;
		status = caServerProcess (pRsrv, &delay);
		assert (status == S_cas_success);
	}
}


/*
 * casSchedualBeacon()
 */
void casSchedualBeacon(struct rsrv *pRsrv)
{
	fdmgrAlarm 	*tmp;
	struct timeval	tv;

	tv.tv_sec = pRsrv->delayToNextBeacon.tv_sec;
	tv.tv_usec = pRsrv->delayToNextBeacon.tv_usec;
	tmp = fdmgr_add_timeout(
			pRsrv->osSpecific.pfdctx,
			&tv,
			casSendBeacon,
			pRsrv);
	if(!tmp){
		ca_printf("CAS: Unable to keep beacon going\n");
	}
}


/*
 * casOSSpecificClientInit()
 */
caStatus casOSSpecificClientInit(struct client *client)
{
	client->cc.osSpecific.eventQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!client->cc.osSpecific.eventQLock){
		return S_cas_noMemory;
	}
	client->cc.osSpecific.chanQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!client->cc.osSpecific.chanQLock){
		casOSSpecificClientDelete(client);
		return S_cas_noMemory;
	}
	client->cc.osSpecific.putNotifyLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!client->cc.osSpecific.putNotifyLock){
		casOSSpecificClientDelete(client);
		return S_cas_noMemory;
	}
	client->cc.osSpecific.lock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!client->cc.osSpecific.lock){
		casOSSpecificClientDelete(client);
		return S_cas_noMemory;
	}
	client->cc.osSpecific.accessRightsQLock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!client->cc.osSpecific.accessRightsQLock){
		casOSSpecificClientDelete(client);
		return S_cas_noMemory;
	}
	return S_cas_success;
}


/*
 * casOSSpecificClientDelete()
 */
void casOSSpecificClientDelete(struct client *client)
{
	int status;

	if(client->cc.osSpecific.eventQLock){
		status = semDelete(client->cc.osSpecific.eventQLock);
		assert(status==OK);
	}
	if(client->cc.osSpecific.chanQLock){
		status = semDelete(client->cc.osSpecific.chanQLock);
		assert(status==OK);
	}
	if(client->cc.osSpecific.putNotifyLock){
		status = semDelete(client->cc.osSpecific.putNotifyLock);
		assert(status==OK);
	}
	if(client->cc.osSpecific.lock){
		status = semDelete(client->cc.osSpecific.lock);
		assert(status==OK);
	}
	if(client->cc.osSpecific.accessRightsQLock){
		status = semDelete(client->cc.osSpecific.accessRightsQLock);
		assert(status==OK);
	}
}


/*
 * caeqOSSpecificInit()
 */
caStatus caeqOSSpecificInit(caEventUser *evuser)
{
	evuser->osSpecific.lock = semMCreate(MUTEX_SEM_OPTIONS);
	if(!evuser->osSpecific.lock){
		return S_cas_noMemory;
	}

        evuser->osSpecific.ppendsem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
        if(!evuser->osSpecific.ppendsem){
		semDelete(evuser->osSpecific.lock);
		return S_cas_noMemory;
        }
	return S_cas_success;
}


/*
 * caeqOSSpecificDelete()
 */
void caeqOSSpecificDelete(caEventUser *evuser)
{
	int status;

	if(taskIdVerify(evuser->osSpecific.taskid)==OK){
        	taskwdRemove(evuser->osSpecific.taskid);
		taskDelete(evuser->osSpecific.taskid);
	}
	status = semDelete(evuser->osSpecific.lock);
	assert(status == OK);
	status = semDelete(evuser->osSpecific.ppendsem);
	assert(status == OK);
}


/*
 * caeqOSSpecificPVInit ()
 */
caStatus caeqOSSpecificPVInit (casPVInUse *pPV)
{
	pPV->osSpecific.lock = semMCreate(MUTEX_SEM_OPTIONS);
	if (!pPV->osSpecific.lock) {
		return S_cas_noMemory;
	}
	return S_cas_success;
}


/*
 * caeqOSSpecificPVDelete ()
 */
void caeqOSSpecificPVDelete (casPVInUse *pPV)
{
	int	status;

	status = semDelete (pPV->osSpecific.lock);
	assert (status == OK);
}


/*
 * caeqOSSpecificEventNotify()
 */
void caeqOSSpecificEventNotify(caEventUser *evuser)
{
	int	status;

	status = semGive(evuser->osSpecific.ppendsem);
	assert(status == OK);
}



/*
 * caeqStartEvents()
 */
int caeqStartEvents(
struct event_user       *evuser,
char                    *taskname,      /* defaulted if NULL */
void                    (*init_func)(),
int                     init_func_arg,
int                     priority_offset
)
{
        int             status;
        int             taskpri;

        /* only one ca_pend_event thread may be started for each evuser ! */
        while(!vxTas(&evuser->osSpecific.pendlck))
                return ERROR;

        status = taskPriorityGet(taskIdSelf(), &taskpri);
        if(status == ERROR){
                return ERROR;
	}

        taskpri += priority_offset;


        if(!taskname){
                taskname = EVENT_PEND_NAME;
	}

        status = taskSpawn(
                taskname,
                taskpri,
                EVENT_PEND_OPT,
                EVENT_PEND_STACK,
                event_task,
                (int)evuser,
                (int)init_func,
                (int)init_func_arg,
                0,0,0,0,0,0,0);
        if(status == ERROR){
                return ERROR;
	}

        evuser->osSpecific.taskid = status;

        return OK;
}



/*
 * EVENT_TASK()
 */
LOCAL int event_task(
struct event_user       *evuser,
void                    (*init_func)(int init_func_arg),
int                     init_func_arg
)
{
        int                     status;

        taskwdInsert((int)taskIdCurrent,NULL,NULL);

        /* init hook */
        if(init_func){
                (*init_func)(init_func_arg);
	}

        /*
         * No need to lock getix as I only allow one thread to call this
         * routine at a time
         */
	while(TRUE){
		caeqEventProcess(evuser);
                status = semTake(evuser->osSpecific.ppendsem, WAIT_FOREVER);
		assert(status == OK);
        }
}


/*
 * caeqOSSpecificClose()
 */
void caeqOSSpecificClose(caEventUser *pEvUser)
{
        return;
}


/*
 * asciiIPAddr
 */
void 	asciiIPAddr (struct in_addr addr, char *pBuf, unsigned bufSize)
{
	char	pName[INET_ADDR_LEN];

	inet_ntoa_b (addr, pName);

	strncpy (pBuf, pName, bufSize);
	pBuf[bufSize-1] = '\0';
}

