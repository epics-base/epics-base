/*asCa.c*/
/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Author:  Marty Kraimer Date:    10-15-93 */

/*This module is separate from asDbLib because CA uses old database access*/
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "cantProceed.h"
#include "errlog.h"
#include "taskwd.h"
#include "callback.h"
#include "db_access.h"
#include "cadef.h"
#include "caerr.h"
#include "caeventmask.h"
#include "alarm.h"
#include "epicsStdioRedirect.h"

#include "epicsExport.h"
#include "asLib.h"
#include "asDbLib.h"
#include "asCa.h"

int asCaDebug = 0;
epicsExportAddress(int,asCaDebug);
static int firstTime = TRUE;
static epicsThreadId threadid=0;
static int caInitializing=FALSE;
static epicsMutexId asCaTaskLock;		/*lock access to task */
static epicsEventId asCaTaskWait;		/*Wait for task to respond*/
static epicsEventId asCaTaskAddChannels;	/*Tell asCaTask to add channels*/
static epicsEventId asCaTaskClearChannels;/*Tell asCaTask to clear channels*/

typedef struct {
    struct dbr_sts_double rtndata;
    chid		chid;
} CAPVT;

static void exceptionCallback(struct exception_handler_args args)
{
    chid        chid = args.chid;
    long        stat = args.stat; /* Channel access status code*/
    const char  *channel;
    const char  *context;
    static char *unknown = "unknown";
    const char *nativeType;
    const char *requestType;
    long nativeCount;
    long requestCount;
    int  readAccess;
    int writeAccess;

    channel = (chid ? ca_name(chid) : unknown);
    context = (args.ctx ? args.ctx : unknown);
    nativeType = dbr_type_to_text((chid ? ca_field_type(chid) : -1));
    requestType = dbr_type_to_text(args.type);
    nativeCount = (chid ? ca_element_count(chid) : 0);
    requestCount = args.count;
    readAccess = (chid ? ca_read_access(chid) : 0);
    writeAccess = (chid ? ca_write_access(chid) : 0);

    errlogPrintf("dbCa:exceptionCallback stat \"%s\" channel \"%s\""
        " context \"%s\"\n"
        " nativeType %s requestType %s"
        " nativeCount %ld requestCount %ld %s %s\n",
        ca_message(stat),channel,context,
        nativeType,requestType,
        nativeCount,requestCount,
        (readAccess ? "readAccess" : "noReadAccess"),
        (writeAccess ? "writeAccess" : "noWriteAccess"));
}

/*connectCallback only handles disconnects*/
static void connectCallback(struct connection_handler_args arg)
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

static void eventCallback(struct event_handler_args arg)
{
    int		caStatus = arg.status;
    chid	chid = arg.chid;
    ASGINP	*pasginp = (ASGINP *)arg.usr;
    ASG		*pasg;
    CAPVT	*pcapvt;
    const struct dbr_sts_double *pdata;

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

static void asCaTask(void)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    CAPVT	*pcapvt;
    int		status;

    taskwdInsert(epicsThreadGetIdSelf(),NULL,NULL);
    SEVCHK(ca_context_create(ca_enable_preemptive_callback),
        "asCaTask calling ca_context_create");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    while(TRUE) { 
        epicsEventMustWait(asCaTaskAddChannels);
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
        SEVCHK(ca_flush_io(),"asCaTask");
	caInitializing = FALSE;
	asComputeAllAsg();
	if(asCaDebug) printf("asCaTask initialized\n");
	epicsEventSignal(asCaTaskWait);
        epicsEventMustWait(asCaTaskClearChannels);
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
	epicsEventSignal(asCaTaskWait);
    }
}
    
void epicsShareAPI asCaStart(void)
{
    if(asCaDebug) printf("asCaStart called\n");
    if(firstTime) {
	firstTime = FALSE;
        asCaTaskLock=epicsMutexMustCreate();
        asCaTaskWait=epicsEventMustCreate(epicsEventEmpty);
        asCaTaskAddChannels=epicsEventMustCreate(epicsEventEmpty);
        asCaTaskClearChannels=epicsEventMustCreate(epicsEventEmpty);
        threadid = epicsThreadCreate("asCaTask",
            (epicsThreadPriorityScanLow - 3),
            epicsThreadGetStackSize(epicsThreadStackBig),
            (EPICSTHREADFUNC)asCaTask,0);
	if(threadid==0) {
	    errMessage(0,"asCaStart: taskSpawn Failure\n");
	}
    }
    epicsMutexMustLock(asCaTaskLock);
    epicsEventSignal(asCaTaskAddChannels);
    epicsEventMustWait(asCaTaskWait);
    if(asCaDebug) printf("asCaStart done\n");
    epicsMutexUnlock(asCaTaskLock);
}

void epicsShareAPI asCaStop(void)
{
    if(threadid==0) return;
    if(asCaDebug) printf("asCaStop called\n");
    epicsMutexMustLock(asCaTaskLock);
    epicsEventSignal(asCaTaskClearChannels);
    epicsEventMustWait(asCaTaskWait);
    if(asCaDebug) printf("asCaStop done\n");
    epicsMutexUnlock(asCaTaskLock);
}

int epicsShareAPI ascar(int level) { return ascarFP(stdout,level);}

int epicsShareAPI ascarFP(FILE *fp,int level)
{
    ASG                *pasg;
    int  n=0,nbad=0;
    enum channel_state state;

    if(!pasbase) {
        fprintf(fp,"access security not started\n");
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
                fprintf(fp,"connected:");
                if(state==cs_never_conn) fprintf(fp,"never ");
                else if(state==cs_prev_conn) fprintf(fp,"prev  ");
                else if(state==cs_conn) fprintf(fp,"yes   ");
                else if(state==cs_closed) fprintf(fp,"closed");
                else fprintf(fp,"unknown");
                fprintf(fp," read:%s write:%s",
                    (ca_read_access(chid) ? "yes" : "no "),
                    (ca_write_access(chid) ? "yes" : "no "));
                fprintf(fp," %s %s\n", ca_name(chid),ca_host_name(chid));
            }
            pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
        }
        pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    fprintf(fp,"%d channels %d not connected\n",n,nbad);
    return(0);
}

void epicsShareAPI ascaStats(int *pchans, int *pdiscon)
{
    ASG *pasg;
    int n = 0;
    int nbad = 0;

    if(!pasbase) {
        if (pchans)  *pchans  = n;
        if (pdiscon) *pdiscon = nbad;
        return;
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while (pasg) {
        ASGINP *pasginp;
        pasginp = (ASGINP *)ellFirst(&pasg->inpList);
        while (pasginp) {
            CAPVT *pcapvt = (CAPVT *)pasginp->capvt;
            chid chid = pcapvt->chid;
            ++n;
            if (ca_state(chid) != cs_conn) ++nbad;
            pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
        }
        pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    if (pchans)  *pchans  = n;
    if (pdiscon) *pdiscon = nbad;
}

