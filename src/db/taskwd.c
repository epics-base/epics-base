/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* taskwd.c */
/* share/src/db  @(#)taskwd.c	1.7  7/11/94 */
/* tasks and subroutines for a general purpose task watchdog */
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
 *
 */

#include	<vxWorks.h>
#include	<vxLib.h>
#include	<stdlib.h>
#include	<stdio.h>
#include 	<ellLib.h>
#include 	<taskLib.h>
#include 	<sysLib.h>

#include        "dbDefs.h"
#include        "errlog.h"
#include        "errMdef.h"
#include        "taskwd.h"
#include        "task_params.h"
#include        "fast_lock.h"

struct task_list {
	ELLNODE		node;
	VOIDFUNCPTR	callback;
	void		*arg;
	union {
		int	tid;
		void	*userpvt;
	} id;
	int		suspended;
};

static ELLLIST list;
static ELLLIST anylist;
static FAST_LOCK lock;
static FAST_LOCK anylock;
static FAST_LOCK alloclock;
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
    FASTLOCKINIT(&anylock);
    FASTLOCKINIT(&alloclock);
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
    pt->id.tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    FASTUNLOCK(&lock);
}

void taskwdAnyInsert(void *userpvt,VOIDFUNCPTR callback,void *arg)
{
    struct task_list *pt;

    FASTLOCK(&anylock);
    pt = allocList();
    ellAdd(&anylist,(void *)pt);
    pt->id.userpvt = userpvt;
    pt->callback = callback;
    pt->arg = arg;
    FASTUNLOCK(&anylock);
}

void taskwdRemove(int tid)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = (struct task_list *)ellFirst(&list);
    while(pt!=NULL) {
	if (tid == pt->id.tid) {
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

void taskwdAnyRemove(void *userpvt)
{
    struct task_list *pt;

    FASTLOCK(&anylock);
    pt = (struct task_list *)ellFirst(&anylist);
    while(pt!=NULL) {
	if (userpvt == pt->id.userpvt) {
	    ellDelete(&anylist,(void *)pt);
	    freeList(pt);
	    FASTUNLOCK(&anylock);
	    return;
	}
	pt = (struct task_list *)ellNext((void *)pt);
    }
    FASTUNLOCK(&anylock);
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
		if(taskIsSuspended(pt->id.tid)) {
		    char *pname;
		    char message[100];

		    pname = taskName(pt->id.tid);
		    if(!pt->suspended) {
			struct task_list *ptany;

			pt->suspended = TRUE;
			sprintf(message,"task %x %s suspended",pt->id.tid,pname);
			errMessage(-1,message);
			ptany = (struct task_list *)ellFirst(&anylist);
			while(ptany) {
			    if(ptany->callback) (ptany->callback)(ptany->arg,pt->id.tid);
			    ptany = (struct task_list *)ellNext((ELLNODE *)ptany);
			}
			if(pt->callback) {
				VOIDFUNCPTR	pcallback = pt->callback;
				void		*arg = pt->arg;

				/*Must allow callback to call taskwdRemove*/
				FASTUNLOCK(&lock);
				(pcallback)(arg);
				/*skip rest because we have unlocked*/
				break;
			}
		    }
		} else {
		    pt->suspended = FALSE;
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

    FASTLOCK(&alloclock);
    if(freeHead) {
	pt = (struct task_list *)freeHead;
	freeHead = freeHead->next;
    } else pt = calloc(1,sizeof(struct task_list));
    if(pt==NULL) {
	errMessage(0,"taskwd failed on call to calloc\n");
	exit(1);
    }
    FASTUNLOCK(&alloclock);
    return(pt);
}

static void freeList(struct task_list *pt)
{
    
    FASTLOCK(&alloclock);
    ((struct freeList *)pt)->next  = freeHead;
    freeHead = (struct freeList *)pt;
    FASTUNLOCK(&alloclock);
}
