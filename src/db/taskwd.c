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


/* Public routines
 *  taskwdInit()			Initialize task watchdor
 *
 *  taskwdInsert(tid,callback,arg)	Insert in lists of tasks to watch
 *	int	tid			Task Id
 *	VOIDFUNCPTR callback		Address of callback routine
 *	void 	*arg			Argument to pass to callback
 *
 *  taskwdRemove(tid)			Remove from list of tasks to watch
 *	
 *	int	tid			Task Id
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<types.h>
#include 	<lstLib.h>
#include 	<taskLib.h>

#include        <dbDefs.h>
#include        <taskwd.h>
#include        <task_params.h>
#include        <fast_lock.h>

struct task_list {
	NODE		node;
	VOIDFUNCPTR	callback;
	void		*arg;
	int		tid;
	int		suspended;
};

static LIST list;
static FAST_LOCK lock;
static int taskwdid=0;
volatile int taskwdOn=TRUE;
struct freeList{
    struct freeList *next;
};
static struct freeList *freeHead=NULL;

/*forward definitions*/
void taskwdTask();
struct task_list *allocList();
void freeList(struct task_list *);

void taskwdInit()
{
    FASTLOCKINIT(&lock);
    lstInit(&list);
    taskwdid = taskSpawn(TASKWD_NAME,TASKWD_PRI,
			TASKWD_OPT,TASKWD_STACK,(FUNCPTR )taskwdTask);
}

void taskwdInsert(int tid,VOIDFUNCPTR callback,void *arg)
{
    struct task_list *pt;

    FASTLOCK(&lock);
    pt = allocList();
    lstAdd(&list,(void *)pt);
    pt->suspended = FALSE;
    pt->tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    FASTUNLOCK(&lock);
}

void taskwdRemove(int tid)
{
    int i;
    struct task_list *pt;

    FASTLOCK(&lock);
    (void *)pt = lstFirst(&list);
    while(pt!=NULL) {
	if (tid == pt->tid) {
	    lstDelete(&list,(void *)pt);
	    freeList(pt);
	    FASTUNLOCK(&lock);
	    return;
	}
	(void *)pt = lstNext((void *)pt);
    }
    FASTUNLOCK(&lock);
    errMessage(-1,"taskwdRemove failed");
}

static void taskwdTask()
{
    struct task_list *pt,*next;

    while(TRUE) {
	if(taskwdOn) {
	    FASTLOCK(&lock);
	    (void *)pt = lstFirst(&list);
	    while(pt!=NULL) {
		(void *)next = lstNext((void *)pt);
		if(taskIsSuspended(pt->tid)) {
		    char *pname;
		    char message[100];

		    pname = taskName(pt->tid);
		    if(!pt->suspended) {
			sprintf(message,"task %d %s suspended",pt->tid,pname);
			errMessage(-1,message);
			if(pt->callback) (pt->callback)(pt->arg);
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


struct task_list *allocList()
{
    struct task_list *pt;

    if(freeHead) {
	(void *)pt = (void *)freeHead;
	freeHead = freeHead->next;
    } else pt = calloc(1,sizeof(struct task_list *));
    if(pt==NULL) {
	errMessage(0,"taskwd failed on call to calloc\n");
	exit(1);
    }
    return(pt);
}

void freeList(struct task_list *pt)
{
    
    ((struct freeList *)pt)->next  = freeHead;
    freeHead = (struct freeList *)pt;
}
