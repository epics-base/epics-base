/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/src/as/asCa.c	*/
/* share/src/as $Id$ */
/* Author:  Marty Kraimer Date:    10-15-93 */

/*This module is separate from asDbLib because CA uses old database access*/
#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <semLib.h>

#include "dbDefs.h"
#include "errlog.h"
#include "taskwd.h"
#include "asDbLib.h"
#include "cadef.h"
#include "caerr.h"
#include "caeventmask.h"
#include "task_params.h"
#include "alarm.h"

int asCaDebug = 0;
extern ASBASE volatile *pasbase;
LOCAL int firstTime = TRUE;
LOCAL int taskid=0;
LOCAL int caInitializing=FALSE;
LOCAL SEM_ID asCaTaskLock;		/*lock access to task */
LOCAL SEM_ID asCaTaskWait;		/*Wait for task to respond*/
LOCAL SEM_ID asCaTaskAddChannels;	/*Tell asCaTask to add channels*/
LOCAL SEM_ID asCaTaskClearChannels;	/*Tell asCaTask to clear channels*/

typedef struct {
    struct dbr_sts_double rtndata;
    chid		chid;
} CAPVT;

static void exceptionCallback(struct exception_handler_args args)
{
    chid        chid = args.chid;
    long        stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (chid ? ca_name(chid) : noname);

    errlogPrintf("asCa:exceptionCallback stat %s channel %s\n",
        ca_message(stat),channel);
}

/*connectCallback only handles disconnects*/
LOCAL void connectCallback(struct connection_handler_args arg)
{
    chid		chid = arg.chid;
    ASGINP		*pasginp = (ASGINP *)ca_puser(chid);
    ASG			*pasg = pasginp->pasg;

    if(ca_state(chid)!=cs_conn) {
	if(!(pasg->inpBad & (1<<pasginp->inpIndex))) {
	    /*was good so lets make it bad*/
	    pasg->inpBad |= (1<<pasginp->inpIndex);
	    if(!caInitializing) asComputeAsg(pasg);
	    if(asCaDebug) printf("as connectCallback disconnect %s\n",
		ca_name(chid));
	}
    }
}

LOCAL void eventCallback(struct event_handler_args arg)
{
    int		caStatus = arg.status;
    chid	chid = arg.chid;
    ASGINP	*pasginp = (ASGINP *)arg.usr;
    ASG		*pasg;
    CAPVT	*pcapvt;
    READONLY struct dbr_sts_double *pdata;

    if(caStatus!=ECA_NORMAL) {
	if(chid) {
	    epicsPrintf("asCa: eventCallback error %s channel %s\n",
	        ca_message(caStatus),ca_name(chid));
	} else {
	    epicsPrintf("asCa: eventCallback error %s chid is null\n",
		ca_message(caStatus));
	}
	return;
    }
    pasg = pasginp->pasg;
    pcapvt = (CAPVT *)pasginp->capvt;
    if(chid!=pcapvt->chid) {
	epicsPrintf("asCa: eventCallback error pcapvt->chid != arg.chid\n");
	return;
    }
    if(ca_state(chid)!=cs_conn || !ca_read_access(chid)) {
	if(!(pasg->inpBad & (1<<pasginp->inpIndex))) {
	    /*was good so lets make it bad*/
	    pasg->inpBad |= (1<<pasginp->inpIndex);
	    if(!caInitializing) asComputeAsg(pasg);
	    if(asCaDebug) {
		printf("as eventCallback %s inpBad ca_state %d"
		    " ca_read_access %d\n",
		    ca_name(chid),ca_state(chid),ca_read_access(chid));
	    }
	}
	return;
    }
    pdata = arg.dbr;
    pcapvt->rtndata = *pdata; /*structure copy*/
    if(pdata->severity==INVALID_ALARM) {
        pasg->inpBad |= (1<<pasginp->inpIndex);
	if(asCaDebug)
	    printf("as eventCallback %s inpBad because INVALID_ALARM\n",
	    ca_name(chid));
    } else {
        pasg->inpBad &= ~((1<<pasginp->inpIndex));
        pasg->pavalue[pasginp->inpIndex] = pdata->value;
	if(asCaDebug)
	    printf("as eventCallback %s inpGood data %f\n",
		ca_name(chid),pdata->value);
    }
    pasg->inpChanged |= (1<<pasginp->inpIndex);
    if(!caInitializing) asComputeAsg(pasg);
}

LOCAL void asCaTask(void)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    CAPVT	*pcapvt;
    int		status;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    while(TRUE) { 
        if(semTake(asCaTaskAddChannels,WAIT_FOREVER)!=OK) {
	    epicsPrintf("asCa semTake error for asCaTaskClearChannels\n");
	    taskSuspend(0);
	}
	caInitializing = TRUE;
	pasg = (ASG *)ellFirst(&pasbase->asgList);
	while(pasg) {
	    pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	    while(pasginp) {
		pasg->inpBad |= (1<<pasginp->inpIndex);
		pcapvt = pasginp->capvt = asCalloc(1,sizeof(CAPVT));
		/*Note calls connectCallback immediately for local Pvs*/
		status = ca_search_and_connect(pasginp->inp,&pcapvt->chid,
		    connectCallback,pasginp);
		if(status!=ECA_NORMAL) {
		    epicsPrintf("asCa ca_search_and_connect error %s\n",
			ca_message(status));
		}
		/*Note calls eventCallback immediately  for local Pvs*/
		status = ca_add_event(DBR_STS_DOUBLE,pcapvt->chid,
		    eventCallback,pasginp,0);
		if(status!=ECA_NORMAL) {
		    epicsPrintf("asCa ca_add_event error %s\n",
			ca_message(status));
		}
		pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	    }
	    pasg = (ASG *)ellNext((ELLNODE *)pasg);
	}
	asComputeAllAsg();
	caInitializing = FALSE;
	if(asCaDebug) printf("asCaTask initialized\n");
	semGive(asCaTaskWait);
	while(TRUE) {
	    if(semTake(asCaTaskClearChannels,NO_WAIT)==OK) break;
	    ca_pend_event(2.0);
	}
	pasg = (ASG *)ellFirst(&pasbase->asgList);
	while(pasg) {
	    pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	    while(pasginp) {
		pcapvt = (CAPVT *)pasginp->capvt;
		status = ca_clear_channel(pcapvt->chid);
		if(status!=ECA_NORMAL) {
		    epicsPrintf("asCa ca_clear_channel error %s\n",
			ca_message(status));
		}
		free(pasginp->capvt);
		pasginp->capvt = 0;
		pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	    }
	    pasg = (ASG *)ellNext((ELLNODE *)pasg);
	}
	if(asCaDebug) printf("asCaTask has cleared all channels\n");
	semGive(asCaTaskWait);
    }
}
    
void asCaStart(void)
{
    if(asCaDebug) printf("asCaStart called\n");
    if(firstTime) {
	firstTime = FALSE;
        if((asCaTaskLock=semBCreate(SEM_Q_FIFO,SEM_FULL))==NULL)
	    epicsPrintf("asCa semBCreate failure\n");
        if((asCaTaskWait=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	    epicsPrintf("asCa semBCreate failure\n");
        if((asCaTaskAddChannels=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	    epicsPrintf("asCa semBCreate failure\n");
        if((asCaTaskClearChannels=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	    epicsPrintf("asCa semBCreate failure\n");
	taskid = taskSpawn("asCaTask",CA_CLIENT_PRI-1,VX_FP_TASK,
	    CA_CLIENT_STACK, (FUNCPTR)asCaTask,0,0,0,0,0,0,0,0,0,0);
	if(taskid==ERROR) {
	    errMessage(0,"asCaStart: taskSpawn Failure\n");
	}
    }
    if(semTake(asCaTaskLock,WAIT_FOREVER)!=OK)
	epicsPrintf("asCa semTake error\n");
    semGive(asCaTaskAddChannels);
    if(semTake(asCaTaskWait,WAIT_FOREVER)!=OK)
	epicsPrintf("asCa semTake error\n");
    if(asCaDebug) printf("asCaStart done\n");
    semGive(asCaTaskLock);
}

void asCaStop(void)
{
    if(taskid==0 || taskid==ERROR) return;
    if(asCaDebug) printf("asCaStop called\n");
    if(semTake(asCaTaskLock,WAIT_FOREVER)!=OK)
	epicsPrintf("asCa semTake error\n");
    semGive(asCaTaskClearChannels);
    if(semTake(asCaTaskWait,WAIT_FOREVER)!=OK)
	epicsPrintf("asCa semTake error\n");
    if(asCaDebug) printf("asCaStop done\n");
    semGive(asCaTaskLock);
}

int ascar(int level)
{
    ASG                *pasg;
    int  n=0,nbad=0;
    enum channel_state state;

    if(!pasbase) {
        printf("access security not started\n");
        return(0);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
        ASGINP *pasginp;
        pasginp = (ASGINP *)ellFirst(&pasg->inpList);
        while(pasginp) {
            CAPVT *pcapvt = (CAPVT *)pasginp->capvt;
            chid chid = pcapvt->chid;
            pcapvt = pasginp->capvt;
            ++n;
            state = ca_state(chid);
            if(state!=cs_conn) ++nbad;
            if(level>1 || (level==1 && state!=cs_conn)) {
                printf("connected:");
                if(state==cs_never_conn) printf("never ");
                else if(state==cs_prev_conn) printf("prev  ");
                else if(state==cs_conn) printf("yes   ");
                else if(state==cs_closed) printf("closed");
                else printf("unknown");
                printf(" read:%s write:%s",
                    (ca_read_access(chid) ? "yes" : "no "),
                    (ca_write_access(chid) ? "yes" : "no "));
                printf(" %s %s\n", ca_name(chid),ca_host_name(chid));
            }
            pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
        }
        pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    printf("%d channels %d not connected\n",n,nbad);
    return(0);
}
