/* taskwd.c */
/* share/src/db  $Id$ */

/* tasks and subroutines for a general purpose task watchdog */
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
 * .01	07-24-91	mrk	Replacement for special purpose scan watchdog
 */

#include	<vxWorks.h>
#include	<vxLib.h>
#include	<stdlib.h>
#include	<stdio.h>
#include 	<ellLib.h>
#include 	<taskLib.h>

#include        <dbDefs.h>
#include        <errMdef.h>
#include        <taskwd.h>
#include        <task_params.h>
#include        <fast_lock.h>

struct task_list {
	ELLNODE		node;
	VOIDFUNCPTR	callback;
	void		*arg;
	int		tid;
	int		suspended;
};

static ELLLIST list;
static ELLLIST anylist;
static FAST_LOCK lock;
static int taskwdid=0;
volatile int taskwdOn=TRUE;
struct freeList{
    struct freeList *next;
};
static struct freeList *freeHead=NULL;

/*forward definitions*/
static void taskwdTask(void);
static struct task_list *allocList(void);
static void freeList(struct task_list *pt);

void taskwdInit()
{
    FASTLOCKINIT(&lock);
    ellInit(&list);
    ellInit(&anylist);
    taskwdid = taskSpawn(TASKWD_NAME,TASKWD_PRI,
			TASKWD_OPT,TASKWD_STACK,(FUNCPTR )taskwdTask,
			0,0,0,0,0,0,0,0,0,0);
}

void taskwdInsert(int tid,VOIDFUNCPTR callback,void *arg)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = allocList();
    ellAdd(&list,(void *)pt);
    pt->suspended = FALSE;
    pt->tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    FASTUNLOCK(&lock);
}

void taskwdAnyInsert(int tid,VOIDFUNCPTR callback,void *arg)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = allocList();
    ellAdd(&anylist,(void *)pt);
    pt->tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    FASTUNLOCK(&lock);
}

void taskwdRemove(int tid)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = (struct task_list *)ellFirst(&list);
    while(pt!=NULL) {
	if (tid == pt->tid) {
	    ellDelete(&list,(void *)pt);
	    freeList(pt);
	    FASTUNLOCK(&lock);
	    return;
	}
	pt = (struct task_list *)ellNext((ELLNODE *)pt);
    }
    FASTUNLOCK(&lock);
    errMessage(-1,"taskwdRemove failed");
}

void taskwdAnyRemove(int tid)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = (struct task_list *)ellFirst(&anylist);
    while(pt!=NULL) {
	if (tid == pt->tid) {
	    ellDelete(&anylist,(void *)pt);
	    freeList(pt);
	    FASTUNLOCK(&lock);
	    return;
	}
	pt = (struct task_list *)ellNext((void *)pt);
    }
    FASTUNLOCK(&lock);
    errMessage(-1,"taskwdanyRemove failed");
}

static void taskwdTask(void)
{
    struct task_list *pt,*next;

    while(TRUE) {
	if(taskwdOn) {
	    FASTLOCK(&lock);
	    pt = (struct task_list *)ellFirst(&list);
	    while(pt) {
		next = (struct task_list *)ellNext((void *)pt);
		if(taskIsSuspended(pt->tid)) {
		    char *pname;
		    char message[100];

		    pname = taskName(pt->tid);
		    if(!pt->suspended) {
			struct task_list *ptany,*anynext;

			sprintf(message,"task %x %s suspended",pt->tid,pname);
			errMessage(-1,message);
			if(pt->callback) (pt->callback)(pt->arg);
			ptany = (struct task_list *)ellFirst(&anylist);
			while(ptany) {
			    if(ptany->callback) (ptany->callback)(ptany->arg,pt->tid);
			    ptany = (struct task_list *)ellNext((ELLNODE *)ptany);
			}
		    }
		    pt->suspended = TRUE;
		}
		pt = next;
	    }
	    FASTUNLOCK(&lock);
	}
	taskDelay(TASKWD_DELAY*vxTicksPerSecond);
    }
}


static struct task_list *allocList(void)
{
    struct task_list *pt;

    if(freeHead) {
	(void *)pt = (void *)freeHead;
	freeHead = freeHead->next;
    } else pt = calloc(1,sizeof(struct task_list));
    if(pt==NULL) {
	errMessage(0,"taskwd failed on call to calloc\n");
	exit(1);
    }
    return(pt);
}

static void freeList(struct task_list *pt)
{
    
    ((struct freeList *)pt)->next  = freeHead;
    freeHead = (struct freeList *)pt;
}
