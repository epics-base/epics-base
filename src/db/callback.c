/* callback.c */
/* share/src/db  @(#)callback.c	1.7  9/14/93 */

/* general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "dbDefs.h"
#include "osiSem.h"
#include "osiThread.h"
#include "osiInterrupt.h"
#include "osiRing.h"
#include "errlog.h"
#include "callback.h"
#include "dbAccess.h"
#include "recSup.h"
#include "taskwd.h"
#include "errMdef.h"
#include "dbCommon.h"
#include "dbLock.h"

int callbackQueueSize = 2000;
static semBinaryId callbackSem[NUM_CALLBACK_PRIORITIES];
static ringId callbackQ[NUM_CALLBACK_PRIORITIES];
static threadId callbackTaskId[NUM_CALLBACK_PRIORITIES];
static int ringOverflow[NUM_CALLBACK_PRIORITIES];
volatile int callbackRestart=FALSE;

static int priorityValue[NUM_CALLBACK_PRIORITIES] = {0,1,2};

/* forward references */
static void wdCallback(void *ind); /*callback from taskwd*/
static void start(int ind); /*start or restart a callbackTask*/

/*public routines */
int callbackSetQueueSize(int size)
{
    callbackQueueSize = size;
    return(0);
}

long callbackInit()
{
    int i;

    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
	start(i);
    }
    return(0);
}

/* Routine which places requests into callback queue*/
/* This routine can be called from interrupt routine*/
void callbackRequest(CALLBACK *pcallback)
{
    int priority = pcallback->priority;
    int nput;
    int lockKey;

    if(priority<0 || priority>=(NUM_CALLBACK_PRIORITIES)) {
	epicsPrintf("callbackRequest called with invalid priority\n");
	return;
    }
    if(ringOverflow[priority]) return;
    lockKey = interruptLock();
    nput = ringPut(callbackQ[priority],(void *)&pcallback,sizeof(pcallback));
    interruptUnlock(lockKey);
    if(nput!=sizeof(pcallback)){
	epicsPrintf("callbackRequest ring buffer full\n");
	ringOverflow[priority] = TRUE;
    }
    semBinaryGive(callbackSem[priority]);
    return;
}

/* General purpose callback task */
static void callbackTask(int *ppriority)
{
    int priority = *ppriority;
    CALLBACK *pcallback;
    int nget;

    ringOverflow[priority] = FALSE;
    while(TRUE) {
	/* wait for somebody to wake us up */
        semBinaryMustTake(callbackSem[priority]);
        while(TRUE) {
	    nget = ringGet(callbackQ[priority],
                (void *)&pcallback,sizeof(pcallback));
            if(nget==0) break;
	    if(nget!=sizeof(pcallback)) {
		errMessage(0,"ringGet failed in callbackTask");
		threadSuspend(threadGetIdSelf());
	    }
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

    callbackSem[ind] = semBinaryMustCreate(semEmpty);
    if(ind==0) priority = threadPriorityScanLow - 1;
    else if(ind==1) priority = threadPriorityScanLow +4;
    else if(ind==2) priority = threadPriorityScanHigh + 1;
    else {
	errMessage(0,"callback start called with illegal priority\n");
	return;
    }
    if((callbackQ[ind]=ringCreate(sizeof(CALLBACK *)*callbackQueueSize)) == 0) 
	errMessage(0,"ringCreate failed while starting a callback task");
    sprintf(taskName,"cb%s",priorityName[ind]);
    callbackTaskId[ind] = threadCreate(taskName,priority,
        threadGetStackSize(threadStackBig),(THREADFUNC)callbackTask,
        &priorityValue[ind]);
    if(callbackTaskId[ind]==0) {
	errMessage(0,"Failed to spawn a callback task");
	return;
    }
    taskwdInsert(callbackTaskId[ind],wdCallback,(void *)&priorityValue[ind]);
}


static void wdCallback(void *pind)
{
    int ind = *(int *)pind;
    taskwdRemove(callbackTaskId[ind]);
    if(!callbackRestart)return;
    threadDestroy(callbackTaskId[ind]);
    semBinaryDestroy(callbackSem[ind]);
    ringDelete(callbackQ[ind]);
    start(ind);
}

static void ProcessCallback(CALLBACK *pCallback)
{
    dbCommon    *pRec;

    callbackGetUser(pRec, pCallback);
    dbScanLock(pRec);
    (*pRec->rset->process)(pRec);
    dbScanUnlock(pRec);
}
void callbackRequestProcessCallback(CALLBACK *pCallback,
	int Priority, void *pRec)
{
    callbackSetCallback(ProcessCallback, pCallback);
    callbackSetPriority(Priority, pCallback);
    callbackSetUser(pRec, pCallback);
    callbackRequest(pCallback);
}


