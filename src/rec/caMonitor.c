#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <rngLib.h>
#include <ellLib.h>
#include <vxLib.h>
#include <dbDefs.h>
#include <taskwd.h>
#include <fast_lock.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#include <calink.h>
#include <task_params.h>
#include <freeList.h>
#include <caMonitor.h>

#define QUEUE_SIZE 256
LOCAL int	taskid=0;
LOCAL ELLLIST	capvtList;
LOCAL RING_ID	ringQ;;
LOCAL FAST_LOCK	lock;
LOCAL void	*freeListPvt;

typedef enum {cmdNone,cmdAdd,cmdRemove} COMMAND;

typedef struct {
    ELLNODE		node;
    CAMONITOR		*pcamonitor;
    chid		chid;
    evid		evid;
    COMMAND		cmd;
    struct dbr_sts_double rtndata; /*Not currently used */
} CAPVT;

void caMonitorTask(void);

LOCAL void eventCallback(struct event_handler_args eha)
{
    struct dbr_sts_double *pdata = eha.dbr;
    CAPVT	*pcapvt;
    CAMONITOR	*pcamonitor;
    
    pcapvt = (CAPVT *)eha.usr;
    pcamonitor = pcapvt->pcamonitor;
    (pcamonitor->callback)(pcamonitor);
}
    
LOCAL void caMonitorStart(void)
{
    FASTLOCKINIT(&lock);
    freeListInitPvt(&freeListPvt,sizeof(CAPVT),1);
    if((ringQ = rngCreate(sizeof(void *) * QUEUE_SIZE)) == NULL) {
	errMessage(0,"caMonitorStart failed");
	exit(1);
    }
    ellInit(&capvtList);
    taskid = taskSpawn("caMonitorTask",CA_CLIENT_PRI-1,VX_FP_TASK,
	CA_CLIENT_STACK,(FUNCPTR)caMonitorTask,0,0,0,0,0,0,0,0,0,0);
    if(taskid==ERROR) {
	errMessage(0,"caMonitorStart: taskSpawn Failure\n");
    }
}

long caMonitorAdd(CAMONITOR *pcamonitor)
{
    CAPVT	*pcapvt;

    if(!taskid) caMonitorStart();
    FASTLOCK(&lock);
    pcapvt = freeListCalloc(freeListPvt);
    pcamonitor->caMonitorPvt = pcapvt;
    pcapvt->pcamonitor = pcamonitor;
    pcapvt->cmd = cmdAdd;
    if(rngBufPut(ringQ,(void *)&pcapvt,sizeof(pcapvt))
        !=sizeof(pcamonitor)) errMessage(0,"caMonitorAdd: rngBufPut error");
    ellAdd(&capvtList,(void *)pcapvt);
    FASTUNLOCK(&lock);
}

long caMonitorDelete(CAMONITOR *pcamonitor)
{
    CAPVT	*pcapvt = pcamonitor->caMonitorPvt;

    FASTLOCK(&lock);
    pcapvt->cmd = cmdRemove;
    if(rngBufPut(ringQ,(void *)&pcapvt,sizeof(pcapvt))
	!=sizeof(pcamonitor)) errMessage(0,"caMonitorDelete: rngBufPut error");
    FASTUNLOCK(&lock);
}

/*LOCAL */
void caMonitorTask(void)
{
    CAPVT	*pcapvt;
    CAMONITOR	*pcamonitor;
    int		status;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    while(TRUE) {
	while (rngNBytes(ringQ)>=sizeof(pcapvt)){
	    if(rngBufGet(ringQ,(void *)&pcapvt,sizeof(pcapvt))
	    !=sizeof(pcapvt)) {
		errMessage(0,"caMonitorTask: rngBufGet error");
		continue;
	    }
    	    FASTLOCK(&lock);
	    pcamonitor = pcapvt->pcamonitor;
	    if(pcapvt->cmd==cmdAdd) {
		SEVCHK(ca_build_and_connect(pcamonitor->channame,TYPENOTCONN,0,
		    &pcapvt->chid,0,NULL,pcapvt),
		    "ca_build_and_connect");
		SEVCHK(ca_add_event(DBR_STS_DOUBLE,pcapvt->chid,
		    eventCallback,pcapvt,&pcapvt->evid),
		    "ca_add_event");
	    } else {/*must be cmdRemove*/
		SEVCHK(ca_clear_channel(pcapvt->chid),"ca_clear_channel");
		pcapvt->cmd=cmdNone;
		ellDelete(&capvtList,(void *)pcapvt);
    		freeListFree(freeListPvt,pcapvt);
	    }
	    pcapvt->cmd=cmdNone;
    	    FASTUNLOCK(&lock);
	}
	status = ca_pend_event(.1);
	if(status!=ECA_NORMAL && status!=ECA_TIMEOUT)
	    SEVCHK(status,"ca_pend_event");
    }
}

static void myCallback(struct caMonitor *pcamonitor)
{
    printf("myCallback: %s\n",pcamonitor->channame);
}

int testCaMonitor(char *name)
{
    CAMONITOR	*pcamonitor;
    long	status;

    pcamonitor = calloc(1,sizeof(CAMONITOR));
    pcamonitor->channame = calloc(1,strlen(name)+1);
    pcamonitor->callback = myCallback;
    strcpy(pcamonitor->channame,name);
    status = caMonitorAdd(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    return(0);
}
