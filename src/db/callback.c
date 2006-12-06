/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* callback.c */
/* share/src/db  @(#)callback.c	1.7  9/14/93 */

/* general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "dbDefs.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsInterrupt.h"
#include "epicsTimer.h"
#include "epicsRingPointer.h"
#include "errlog.h"
#include "dbStaticLib.h"
#include "dbBase.h"
#include "link.h"
#include "dbFldTypes.h"
#include "recSup.h"
#include "taskwd.h"
#include "errMdef.h"
#include "dbCommon.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbAccessDefs.h"
#include "dbLock.h"
#include "callback.h"

int callbackQueueSize = 2000;
static epicsEventId callbackSem[NUM_CALLBACK_PRIORITIES];
static epicsRingPointerId callbackQ[NUM_CALLBACK_PRIORITIES];
static epicsThreadId callbackTaskId[NUM_CALLBACK_PRIORITIES];
static int ringOverflow[NUM_CALLBACK_PRIORITIES];
static void callbackInitPvt(void *);
volatile int callbackRestart=FALSE;

static int priorityValue[NUM_CALLBACK_PRIORITIES] = {0,1,2};

/*for Delayed Requests */
static void notify(void *pPrivate);
static epicsTimerQueueId timerQueue;


/* forward references */
static void wdCallback(void *ind); /*callback from taskwd*/
static void start(int ind); /*start or restart a callbackTask*/

/*public routines */
int epicsShareAPI callbackSetQueueSize(int size)
{
    callbackQueueSize = size;
    return(0);
}

static void callbackInitPvt(void *arg)
{
    int i;

    timerQueue = epicsTimerQueueAllocate(0,epicsThreadPriorityScanHigh);
    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
	start(i);
    }
}

void epicsShareAPI callbackInit(void)
{
    static epicsThreadOnceId callbackOnceFlag = EPICS_THREAD_ONCE_INIT;
    void *arg = 0;
    epicsThreadOnce(&callbackOnceFlag,callbackInitPvt,arg);
}

/* Routine which places requests into callback queue*/
/* This routine can be called from interrupt routine*/
void epicsShareAPI callbackRequest(CALLBACK *pcallback)
{
    int priority = pcallback->priority;
    int pushOK;
    int lockKey;

    if(priority<0 || priority>=(NUM_CALLBACK_PRIORITIES)) {
	epicsPrintf("callbackRequest called with invalid priority\n");
	return;
    }
    if(ringOverflow[priority]) return;
    lockKey = epicsInterruptLock();
    pushOK = epicsRingPointerPush(callbackQ[priority],(void *)pcallback);
    epicsInterruptUnlock(lockKey);
    if(!pushOK) {
	epicsPrintf("callbackRequest ring buffer full\n");
	ringOverflow[priority] = TRUE;
    }
    epicsEventSignal(callbackSem[priority]);
    return;
}

/* General purpose callback task */
static void callbackTask(int *ppriority)
{
    int priority = *ppriority;
    CALLBACK *pcallback;

    taskwdInsert(epicsThreadGetIdSelf(),
        wdCallback,(void *)&priorityValue[*ppriority]);
    ringOverflow[priority] = FALSE;
    while(TRUE) {
	/* wait for somebody to wake us up */
        epicsEventMustWait(callbackSem[priority]);
        while(TRUE) {
            if(!(pcallback = (CALLBACK *)
                epicsRingPointerPop(callbackQ[priority]))) break;
	    ringOverflow[priority] = FALSE;
	    (*pcallback->callback)(pcallback);
	}
    }
}

static char *priorityName[3] = {"Low","Medium","High"};
static void start(int ind)
{
    unsigned int priority;
    char taskName[20];

    callbackSem[ind] = epicsEventMustCreate(epicsEventEmpty);
    if(ind==0) priority = epicsThreadPriorityScanLow - 1;
    else if(ind==1) priority = epicsThreadPriorityScanLow +4;
    else if(ind==2) priority = epicsThreadPriorityScanHigh + 1;
    else {
	errMessage(0,"callback start called with illegal priority\n");
	return;
    }
    if((callbackQ[ind]=epicsRingPointerCreate(callbackQueueSize)) == 0) 
	errMessage(0,"epicsRingPointerCreate failed while starting a callback task");
    sprintf(taskName,"cb%s",priorityName[ind]);
    callbackTaskId[ind] = epicsThreadCreate(taskName,priority,
        epicsThreadGetStackSize(epicsThreadStackBig),(EPICSTHREADFUNC)callbackTask,
        &priorityValue[ind]);
    if(callbackTaskId[ind]==0) {
	errMessage(0,"Failed to spawn a callback task");
	return;
    }
}


static void wdCallback(void *pind)
{
    int ind = *(int *)pind;
    taskwdRemove(callbackTaskId[ind]);
    if(!callbackRestart)return;
    epicsEventDestroy(callbackSem[ind]);
    epicsRingPointerDelete(callbackQ[ind]);
    start(ind);
}

static void ProcessCallback(CALLBACK *pcallback)
{
    dbCommon    *pRec;

    callbackGetUser(pRec, pcallback);
    dbScanLock(pRec);
    (*pRec->rset->process)(pRec);
    dbScanUnlock(pRec);
}
void epicsShareAPI callbackRequestProcessCallback(CALLBACK *pcallback,
	int Priority, void *pRec)
{
    callbackSetCallback(ProcessCallback, pcallback);
    callbackSetPriority(Priority, pcallback);
    callbackSetUser(pRec, pcallback);
    callbackRequest(pcallback);
}

static void notify(void *pPrivate)
{
    CALLBACK *pcallback = (CALLBACK *)pPrivate;
    callbackRequest(pcallback);
}

void epicsShareAPI callbackRequestDelayed(CALLBACK *pcallback,double seconds)
{
    epicsTimerId timer = (epicsTimerId)pcallback->timer;

    if(timer==0) {
        timer = epicsTimerQueueCreateTimer(timerQueue,notify,(void *)pcallback);
        pcallback->timer = timer;
    }
    epicsTimerStartDelay(timer,seconds);
}

void epicsShareAPI callbackRequestProcessCallbackDelayed(CALLBACK *pcallback,
    int Priority, void *pRec,double seconds)
{
    callbackSetCallback(ProcessCallback, pcallback);
    callbackSetPriority(Priority, pcallback);
    callbackSetUser(pRec, pcallback);
    callbackRequestDelayed(pcallback,seconds);
}
