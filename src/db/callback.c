/* callback.c */
/* share/src/db  @(#)callback.c	1.7  9/14/93 */

/* general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
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
 * Modification Log:
 * -----------------
 * .01	12-12-91	mrk	moved from dbScan.c to callback.c
 * .02	04-23-92	jba	Fixed test on priority
 * .03	06-28-93	mrk	In callbackRequest replaced errMessage by logMsg
*/

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<semLib.h>
#include	<rngLib.h>
#include 	<logLib.h>
#include 	<intLib.h>

#include	"dbDefs.h"
#include	"errlog.h"
#include	"callback.h"
#include	"dbAccess.h"
#include	"recSup.h"
#include	"taskwd.h"
#include	"errMdef.h"
#include	"dbCommon.h"
#include	"dbLock.h"
#include	"task_params.h"

int callbackQueueSize = 2000;
static SEM_ID callbackSem[NUM_CALLBACK_PRIORITIES];
static RING_ID callbackQ[NUM_CALLBACK_PRIORITIES];
static int callbackTaskId[NUM_CALLBACK_PRIORITIES];
static int ringOverflow[NUM_CALLBACK_PRIORITIES];
volatile int callbackRestart=FALSE;

/* forward references */
static void wdCallback(long ind); /*callback from taskwd*/
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
    int lockKey;
    int nput;
    static int status;

    if(priority<0 || priority>=(NUM_CALLBACK_PRIORITIES)) {

	logMsg("callbackRequest called with invalid priority\n",0,0,0,0,0,0);
	return;
    }
    if(ringOverflow[priority]) return;
    lockKey = intLock();
    nput = rngBufPut(callbackQ[priority],(void *)&pcallback,sizeof(pcallback));
    intUnlock(lockKey);
    if(nput!=sizeof(pcallback)){
	logMsg("callbackRequest ring buffer full\n",0,0,0,0,0,0);
	ringOverflow[priority] = TRUE;
    }
    if((status=semGive(callbackSem[priority]))!=OK) {
/*semGive randomly returns garbage value*/
/*
	logMsg("semGive returned error in callbackRequest\n",0,0,0,0,0,0);
*/
    }
    return;
}

/* General purpose callback task */
/*static*/
 void callbackTask(int priority)
{
    CALLBACK *pcallback;
    int nget;

    ringOverflow[priority] = FALSE;
    while(TRUE) {
	/* wait for somebody to wake us up */
        if(semTake(callbackSem[priority],WAIT_FOREVER)!=OK ){
		errMessage(0,"semTake returned error in callbackTask\n");
		taskSuspend(0);
	}
	while(rngNBytes(callbackQ[priority])>=sizeof(pcallback)) {
	    nget = rngBufGet(callbackQ[priority],(void *)&pcallback,sizeof(pcallback));
	    if(nget!=sizeof(pcallback)) {
		errMessage(0,"rngBufGet failed in callbackTask");
		taskSuspend(0);
	    }
	    ringOverflow[priority] = FALSE;
	    (*pcallback->callback)(pcallback);
	}
    }
}

static char *priorityName[3] = {"Low","Medium","High"};
static void start(int ind)
{
    int     priority;
    char    taskName[20];

    if((callbackSem[ind] = semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	errMessage(0,"semBcreate failed while starting a callback task\n");
    if(ind==0) priority = CALLBACK_PRI_LOW;
    else if(ind==1) priority = CALLBACK_PRI_MEDIUM;
    else if(ind==2) priority = CALLBACK_PRI_HIGH;
    else {
	errMessage(0,"semBcreate failed while starting a callback task\n");
	return;
    }
    if((callbackQ[ind]=rngCreate(sizeof(CALLBACK *)*callbackQueueSize)) == NULL) 
	errMessage(0,"rngCreate failed while starting a callback task");
    sprintf(taskName,"cb%s",priorityName[ind]);
    callbackTaskId[ind] = taskSpawn(taskName,priority,
    			CALLBACK_OPT,CALLBACK_STACK,
    			(FUNCPTR)callbackTask,ind,
			0,0,0,0,0,0,0,0,0);
    if(callbackTaskId[ind]==ERROR) {
	errMessage(0,"Failed to spawn a callback task");
	return;
    }
    taskwdInsert(callbackTaskId[ind],wdCallback,(void *)(long)ind);
}


static void wdCallback(long ind)
{
    taskwdRemove(callbackTaskId[ind]);
    if(!callbackRestart)return;
    if(taskDelete(callbackTaskId[ind])!=OK)
	errMessage(0,"taskDelete failed while restarting a callback task\n");
    if(semDelete(callbackSem[ind])!=OK)
	errMessage(0,"semDelete failed while restarting a callback task\n");
    rngDelete(callbackQ[ind]);
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


