#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <rngLib.h>
#include <vxLib.h>
#include <dbDefs.h>
#include <taskwd.h>
#include <fast_lock.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#include <calink.h>
#include <task_params.h>
#include <recWaitCa.h>

extern int interruptAccept;

#define QUEUE_SIZE 256
LOCAL int	taskid=0;
LOCAL RING_ID	ringQ;;
LOCAL FAST_LOCK	lock;

typedef enum {cmdNone,cmdAdd,cmdRemove} COMMAND;

typedef struct {
    RECWAITCA		*pcamonitor;
    chid		chid;
    evid		evid;
    COMMAND		cmd;
    int			hasMonitor;
    struct dbr_sts_double rtndata; /*Not currently used */
} CAPVT;

void recWaitCaTask(void);

LOCAL void eventCallback(struct event_handler_args eha)
{
    struct dbr_sts_double *pdata = eha.dbr;
    CAPVT	*pcapvt;
    RECWAITCA	*pcamonitor;
    
    pcapvt = (CAPVT *)eha.usr;
    pcamonitor = pcapvt->pcamonitor;
    (pcamonitor->callback)(pcamonitor, pcamonitor->inputIndex, pdata->value);
}
    
LOCAL void recWaitCaStart(void)
{
    FASTLOCKINIT(&lock);
    if((ringQ = rngCreate(sizeof(void *) * QUEUE_SIZE)) == NULL) {
	errMessage(0,"recWaitCaStart failed");
	exit(1);
    }
    taskid = taskSpawn("recWaitCaTask",CA_CLIENT_PRI-1,VX_FP_TASK,
	CA_CLIENT_STACK,(FUNCPTR)recWaitCaTask,0,0,0,0,0,0,0,0,0,0);
    if(taskid==ERROR) {
	errMessage(0,"recWaitCaStart: taskSpawn Failure\n");
    }
}

long recWaitCaAdd(RECWAITCA *pcamonitor)
{
    CAPVT	*pcapvt;

    if(!taskid) recWaitCaStart();
    FASTLOCK(&lock);
    pcapvt = pcamonitor->recWaitCaPvt;
    if(pcapvt == NULL) {
	pcapvt = calloc(1,sizeof(CAPVT));
	pcamonitor->recWaitCaPvt = pcapvt;
	pcapvt->pcamonitor = pcamonitor;
    }
    pcapvt->cmd = cmdAdd;
    if(rngBufPut(ringQ,(void *)&pcapvt,sizeof(pcapvt))
        !=sizeof(pcamonitor)) errMessage(0,"recWaitCaAdd: rngBufPut error");
    FASTUNLOCK(&lock);
}

long recWaitCaDelete(RECWAITCA *pcamonitor)
{
    CAPVT	*pcapvt = pcamonitor->recWaitCaPvt;

    FASTLOCK(&lock);
    pcapvt->cmd = cmdRemove;
    if(rngBufPut(ringQ,(void *)&pcapvt,sizeof(pcapvt))
	!=sizeof(pcamonitor)) errMessage(0,"recWaitCaDelete: rngBufPut error");
    FASTUNLOCK(&lock);
}

/*LOCAL */
void recWaitCaTask(void)
{
    CAPVT	*pcapvt;
    RECWAITCA	*pcamonitor;
    int		status;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    while(TRUE) {
	while (rngNBytes(ringQ)>=sizeof(pcapvt) && interruptAccept){
	    if(rngBufGet(ringQ,(void *)&pcapvt,sizeof(pcapvt))
	    !=sizeof(pcapvt)) {
		errMessage(0,"recWaitCaTask: rngBufGet error");
		continue;
	    }
    	    FASTLOCK(&lock);
	    pcamonitor = pcapvt->pcamonitor;
	    if(pcapvt->cmd==cmdAdd) {
		if(pcapvt->hasMonitor
		&& (strcmp(pcamonitor->channame,ca_name(pcapvt->chid))!=0)) {
		    SEVCHK(ca_clear_channel(pcapvt->chid),"ca_clear_channel");
		    pcapvt->hasMonitor = FALSE;
		}
		if(!pcapvt->hasMonitor) {
		    SEVCHK(ca_build_and_connect(pcamonitor->channame,
			TYPENOTCONN,0,&pcapvt->chid,0,NULL,pcapvt),
		    "ca_build_and_connect");
		    SEVCHK(ca_add_event(DBR_STS_DOUBLE,pcapvt->chid,
		    	eventCallback,pcapvt,&pcapvt->evid),
		    	"ca_add_event");
		    pcapvt->hasMonitor = TRUE;
		}
	    } else if (pcapvt->cmd==cmdRemove && pcapvt->hasMonitor) {
		SEVCHK(ca_clear_channel(pcapvt->chid),"ca_clear_channel");
		pcapvt->hasMonitor = FALSE;
	    }
    	    FASTUNLOCK(&lock);
	}
	status = ca_pend_event(.1);
	if(status!=ECA_NORMAL && status!=ECA_TIMEOUT)
	    SEVCHK(status,"ca_pend_event");
    }
}

static void myCallback(struct recWaitCa *pcamonitor, char inputIndex, double monData)
{
    printf("myCallback: %s index=%d\n",pcamonitor->channame,
	pcamonitor->inputIndex);
}

int testCaMonitor(char *name,char *name2,int delay)
{
    RECWAITCA	*pcamonitor;
    long	status;

    pcamonitor = calloc(1,sizeof(RECWAITCA));
    pcamonitor->channame = calloc(1,100);
    pcamonitor->callback = myCallback;
    strcpy(pcamonitor->channame,name);
    status = recWaitCaAdd(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    taskDelay(10);
    status = recWaitCaDelete(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    if(delay>0) taskDelay(delay);
    status = recWaitCaAdd(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    taskDelay(10);
    status = recWaitCaDelete(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    if(delay>0) taskDelay(delay);
    if(name2) strcpy(pcamonitor->channame,name2);
    status = recWaitCaAdd(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    taskDelay(10);
    status = recWaitCaDelete(pcamonitor);
    if(status) errMessage(status,"testCaMonitor error");
    if(delay>0) taskDelay(delay);
    return(0);
}
