/* callback.c */
/* share/src/db  $Id$ */

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
*/

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<types.h>
#include	<semLib.h>
#include 	<lstLib.h>

#include	<dbDefs.h>
#include	<callback.h>
#include	<taskwd.h>
#include	<task_params.h>

static SEM_ID callbackSem[NUM_CALLBACK_PRIORITIES];
static int callbackTaskId[NUM_CALLBACK_PRIORITIES];
volatile int callbackRestart=FALSE;
static volatile CALLBACK *head[NUM_CALLBACK_PRIORITIES];
static volatile CALLBACK *tail[NUM_CALLBACK_PRIORITIES];

/* forward references */
void wdCallback();	/*callback from taskwd*/
void start();		/*start or restart a callbackTask*/

/*public routines */
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
long callbackRequest(struct callback *pcallback)
{
    int priority = pcallback->priority;
    int lockKey;

    if(priority<0 || priority>(NUM_CALLBACK_PRIORITIES)) {
	logMsg("callbackRequest called with invalid priority");
	return(-1);
    }
    lockKey = intLock();
    pcallback->next = NULL;
    tail[priority]->next = pcallback;
    if(head[priority]==NULL) head[priority] = pcallback;
    intUnlock(lockKey);
    if(semGive(callbackSem[priority])!=OK)
		logMsg("semGive returned error in callbackRequest\n");
    return(0);
}

/* General purpose callback task */
static void callbackTask(int priority)
{
    volatile CALLBACK *pcallback,*next;
    int lockKey;

    while(TRUE) {
	/* wait for somebody to wake us up */
        if(semTake(callbackSem[priority],WAIT_FOREVER)!=OK )
		logMsg("semTake returned error in callbackRequest\n");

	while(TRUE) {
	    lockKey = intLock();
	    if((pcallback=head[priority])==NULL) break;
	    if((head[priority]=pcallback->next)==NULL) tail[priority]=NULL;
	    intUnlock(lockKey);
	    (*pcallback->callback)(pcallback);
	}
    }
}

static void start(int ind)
{
    char    name[100];
    int     priority;

    head[ind] = tail[ind] = 0;
    if((callbackSem[ind] = semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	logMsg("semBcreate failed while starting a callback task\n");
    sprintf(name,"%s%2.2d",CALLBACK_NAME,ind);
    if(ind==0) priority = CALLBACK_PRI_LOW;
    else if(ind==1) priority = CALLBACK_PRI_MEDIUM;
    else if(ind==2) priority = CALLBACK_PRI_HIGH;
    else {
	logMsg("semBcreate failed while starting a callback task\n");
	return;
    }
    callbackTaskId[ind] = taskSpawn(name,priority,
    			CALLBACK_OPT,CALLBACK_STACK,
    			(FUNCPTR)callbackTask,ind);
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
	logMsg("taskDelete failed while restarting a callback task\n");
    if(semFlush(callbackSem[ind])!=OK)
	logMsg("semFlush failed while restarting a callback task\n");
    start(ind);
}
