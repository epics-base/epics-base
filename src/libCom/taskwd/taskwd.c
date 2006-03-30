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
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "ellLib.h"
#include "errMdef.h"
#include "taskwd.h"

typedef void (*MYFUNCPTR)();

struct task_list {
    ELLNODE	node;
    MYFUNCPTR	callback;
    void	*arg;
    union {
        epicsThreadId tid;
        void     *userpvt;
    } id;
    int suspended;
};

static ELLLIST list;
static ELLLIST anylist;
static epicsMutexId lock;
static epicsMutexId anylock;
static epicsMutexId alloclock;
static epicsThreadId taskwdid=0;
volatile int taskwdOn=TRUE;
struct freeList{
    struct freeList *next;
};
static struct freeList *freeHead=NULL;
/* Task delay times (seconds) */
#define TASKWD_DELAY    6.0

/*forward definitions*/
static void taskwdTask(void);
static struct task_list *allocList(void);
static void freeList(struct task_list *pt);
static void taskwdInitPvt(void *);

static void taskwdInitPvt(void *arg)
{
    lock = epicsMutexMustCreate();
    anylock = epicsMutexMustCreate();
    alloclock = epicsMutexMustCreate();
    ellInit(&list);
    ellInit(&anylist);
    taskwdid = epicsThreadCreate(
        "taskwd",epicsThreadPriorityLow,
         epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)taskwdTask,0);
}
void epicsShareAPI taskwdInit()
{
    static epicsThreadOnceId taskwdOnceFlag = EPICS_THREAD_ONCE_INIT;
    void *arg = 0;
    epicsThreadOnce(&taskwdOnceFlag,taskwdInitPvt,arg);
}

void epicsShareAPI taskwdInsert(epicsThreadId tid,TASKWDFUNCPRR callback,void *arg)
{
    struct task_list *pt;

    taskwdInit();
    if(tid==0) {
       errlogPrintf("taskwdInsert called with null tid\n");
       return;
    }
    epicsMutexMustLock(lock);
    pt = allocList();
    ellAdd(&list,(void *)pt);
    pt->suspended = FALSE;
    pt->id.tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    epicsMutexUnlock(lock);
}

void epicsShareAPI taskwdAnyInsert(void *userpvt,TASKWDANYFUNCPRR callback,void *arg)
{
    struct task_list *pt;

    taskwdInit();
    epicsMutexMustLock(anylock);
    pt = allocList();
    ellAdd(&anylist,(void *)pt);
    pt->id.userpvt = userpvt;
    pt->callback = callback;
    pt->arg = arg;
    epicsMutexUnlock(anylock);
}

void epicsShareAPI taskwdRemove(epicsThreadId tid)
{
    struct task_list *pt;
    char thrName[40];

    taskwdInit();
    epicsMutexMustLock(lock);
    pt = (struct task_list *)ellFirst(&list);
    while(pt!=NULL) {
        if (tid == pt->id.tid) {
            ellDelete(&list,(void *)pt);
            freeList(pt);
            epicsMutexUnlock(lock);
            return;
        }
        pt = (struct task_list *)ellNext(&pt->node);
    }
    epicsMutexUnlock(lock);
    epicsThreadGetName(tid, thrName, 40);
    errlogPrintf("taskwdRemove: Thread %s (%p) not registered!\n",
    		thrName, tid);
}

void epicsShareAPI taskwdAnyRemove(void *userpvt)
{
    struct task_list *pt;

    taskwdInit();
    epicsMutexMustLock(anylock);
    pt = (struct task_list *)ellFirst(&anylist);
    while(pt!=NULL) {
        if (userpvt == pt->id.userpvt) {
            ellDelete(&anylist,(void *)pt);
            freeList(pt);
            epicsMutexUnlock(anylock);
            return;
        }
        pt = (struct task_list *)ellNext(&pt->node);
    }
    epicsMutexUnlock(anylock);
    errlogPrintf("taskwdAnyRemove: Userpvt %p not registered!\n", userpvt);
}

static void taskwdTask(void)
{
    struct task_list *pt,*next;

    while(TRUE) {
        if(taskwdOn) {
            epicsMutexMustLock(lock);
            pt = (struct task_list *)ellFirst(&list);
            while(pt) {
                next = (struct task_list *)ellNext(&pt->node);
                if(epicsThreadIsSuspended(pt->id.tid)) {
                    if(!pt->suspended) {
                        struct task_list *ptany;
                        char thrName[40];

                        pt->suspended = TRUE;
                        epicsThreadGetName(pt->id.tid, thrName, 40);
                        errlogPrintf("Thread %s (%p) suspended\n",
                                    thrName, (void *)pt->id.tid);
                        ptany = (struct task_list *)ellFirst(&anylist);
                        while(ptany) {
                            if(ptany->callback) {
                                TASKWDANYFUNCPRR pcallback = pt->callback;
                                (pcallback)(ptany->arg,pt->id.tid);
                            }
                            ptany = (struct task_list *)ellNext(&ptany->node);
                        }
                        if(pt->callback) {
                            TASKWDFUNCPRR pcallback = pt->callback;
                            void	*arg = pt->arg;

                            /*Must allow callback to call taskwdRemove*/
                            epicsMutexUnlock(lock);
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
            epicsMutexUnlock(lock);
        }
        epicsThreadSleep(TASKWD_DELAY);
    }
}

static struct task_list *allocList(void)
{
    struct task_list *pt;

    epicsMutexMustLock(alloclock);
    if(freeHead) {
        pt = (struct task_list *)freeHead;
        freeHead = freeHead->next;
    } else pt = calloc(1,sizeof(struct task_list));
    while (pt==NULL) {
        errlogPrintf("Thread taskwd suspending: out of memory\n");
        epicsThreadSuspendSelf();
        pt = calloc(1,sizeof(struct task_list));
    }
    epicsMutexUnlock(alloclock);
    return(pt);
}

static void freeList(struct task_list *pt)
{
    
    epicsMutexMustLock(alloclock);
    ((struct freeList *)pt)->next  = freeHead;
    freeHead = (struct freeList *)pt;
    epicsMutexUnlock(alloclock);
}
