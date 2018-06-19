/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* taskwd.c */

/* tasks and subroutines for a general purpose task watchdog */
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

#include <stddef.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "dbDefs.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsStdioRedirect.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "valgrind/valgrind.h"
#include "errlog.h"
#include "ellLib.h"
#include "errMdef.h"
#include "taskwd.h"

struct tNode {
    ELLNODE node;
    epicsThreadId tid;
    TASKWDFUNC callback;
    void *usr;
    int suspended;
};

struct mNode {
    ELLNODE node;
    const taskwdMonitor *funcs;
    void *usr;
};

struct aNode {
    void *key;
    TASKWDANYFUNC callback;
    void *usr;
};

union twdNode {
    struct tNode t;
    struct mNode m;
    struct aNode a;
};

/* Registered Tasks */
static epicsMutexId tLock;
static ELLLIST tList = ELLLIST_INIT;

/* Active Monitors */
static epicsMutexId mLock;
static ELLLIST mList = ELLLIST_INIT;

/* Free List */
static epicsMutexId fLock;
static ELLLIST fList = ELLLIST_INIT;

/* Watchdog task control */
static volatile enum {
    twdctlInit, twdctlRun, twdctlDisable, twdctlExit
} twdCtl;
static epicsEventId loopEvent;
static epicsEventId exitEvent;

/* Task delay times (seconds) */
#define TASKWD_DELAY    6.0


/* forward definitions */
static union twdNode *allocNode(void);
static void freeNode(union twdNode *);

/* Initialization, lazy */

static void twdTask(void *arg)
{
    struct tNode *pt;
    struct mNode *pm;

    while (twdCtl != twdctlExit) {
        if (twdCtl == twdctlRun) {
            epicsMutexMustLock(tLock);
            pt = (struct tNode *)ellFirst(&tList);
            while (pt) {
                int susp = epicsThreadIsSuspended(pt->tid);
                if (susp != pt->suspended) {
                    epicsMutexMustLock(mLock);
                    pm = (struct mNode *)ellFirst(&mList);
                    while (pm) {
                        if (pm->funcs->notify) {
                            pm->funcs->notify(pm->usr, pt->tid, susp);
                        }
                        pm = (struct mNode *)ellNext(&pm->node);
                    }
                    epicsMutexUnlock(mLock);

                    if (susp) {
                        char tName[40];
                        epicsThreadGetName(pt->tid, tName, sizeof(tName));
                        errlogPrintf("Thread %s (%p) suspended\n",
                                    tName, (void *)pt->tid);
                        if (pt->callback) {
                            pt->callback(pt->usr);
                        }
                    }
                    pt->suspended = susp;
                }
                pt = (struct tNode *)ellNext(&pt->node);
            }
            epicsMutexUnlock(tLock);
        }
        epicsEventWaitWithTimeout(loopEvent, TASKWD_DELAY);
    }
    epicsEventSignal(exitEvent);
}


static void twdShutdown(void *arg)
{
    ELLNODE *cur;
    twdCtl = twdctlExit;
    epicsEventSignal(loopEvent);
    epicsEventWait(exitEvent);
    while ((cur = ellGet(&fList)) != NULL) {
        VALGRIND_MEMPOOL_FREE(&fList, cur);
        free(cur);
    }
    VALGRIND_DESTROY_MEMPOOL(&fList);
}

static void twdInitOnce(void *arg)
{
    epicsThreadId tid;

    tLock = epicsMutexMustCreate();
    mLock = epicsMutexMustCreate();
    fLock = epicsMutexMustCreate();
    ellInit(&fList);
    VALGRIND_CREATE_MEMPOOL(&fList, 0, 0);

    twdCtl = twdctlRun;
    loopEvent = epicsEventMustCreate(epicsEventEmpty);
    exitEvent = epicsEventMustCreate(epicsEventEmpty);

    tid = epicsThreadCreate("taskwd", epicsThreadPriorityLow,
         epicsThreadGetStackSize(epicsThreadStackSmall),
         twdTask, NULL);
    if (tid == 0)
        cantProceed("Failed to spawn task watchdog thread\n");

    epicsAtExit(twdShutdown, NULL);
}

void taskwdInit(void)
{
    static epicsThreadOnceId twdOnceFlag = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce(&twdOnceFlag, twdInitOnce, NULL);
}


/* For tasks to be monitored */

void taskwdInsert(epicsThreadId tid, TASKWDFUNC callback, void *usr)
{
    struct tNode *pt;
    struct mNode *pm;

    taskwdInit();
    if (tid == 0)
       tid = epicsThreadGetIdSelf();

    pt = &allocNode()->t;
    pt->tid = tid;
    pt->callback = callback;
    pt->usr = usr;
    pt->suspended = FALSE;

    epicsMutexMustLock(mLock);
    pm = (struct mNode *)ellFirst(&mList);
    while (pm) {
        if (pm->funcs->insert) {
            pm->funcs->insert(pm->usr, tid);
        }
        pm = (struct mNode *)ellNext(&pm->node);
    }
    epicsMutexUnlock(mLock);

    epicsMutexMustLock(tLock);
    ellAdd(&tList, (void *)pt);
    epicsMutexUnlock(tLock);
}

void taskwdRemove(epicsThreadId tid)
{
    struct tNode *pt;
    struct mNode *pm;
    char tName[40];

    taskwdInit();

    if (tid == 0)
       tid = epicsThreadGetIdSelf();

    epicsMutexMustLock(tLock);
    pt = (struct tNode *)ellFirst(&tList);
    while (pt != NULL) {
        if (tid == pt->tid) {
            ellDelete(&tList, (void *)pt);
            epicsMutexUnlock(tLock);
            freeNode((union twdNode *)pt);

            epicsMutexMustLock(mLock);
            pm = (struct mNode *)ellFirst(&mList);
            while (pm) {
                if (pm->funcs->remove) {
                    pm->funcs->remove(pm->usr, tid);
                }
                pm = (struct mNode *)ellNext(&pm->node);
            }
            epicsMutexUnlock(mLock);
            return;
        }
        pt = (struct tNode *)ellNext(&pt->node);
    }
    epicsMutexUnlock(tLock);

    epicsThreadGetName(tid, tName, sizeof(tName));
    errlogPrintf("taskwdRemove: Thread %s (%p) not registered!\n",
        tName, (void *)tid);
}


/* Monitoring API */

void taskwdMonitorAdd(const taskwdMonitor *funcs, void *usr)
{
    struct mNode *pm;

    if (funcs == NULL) return;

    taskwdInit();

    pm = &allocNode()->m;
    pm->funcs = funcs;
    pm->usr = usr;

    epicsMutexMustLock(mLock);
    ellAdd(&mList, (void *)pm);
    epicsMutexUnlock(mLock);
}

void taskwdMonitorDel(const taskwdMonitor *funcs, void *usr)
{
    struct mNode *pm;

    if (funcs == NULL) return;

    taskwdInit();

    epicsMutexMustLock(mLock);
    pm = (struct mNode *)ellFirst(&mList);
    while (pm) {
        if (pm->funcs == funcs && pm->usr == usr) {
            ellDelete(&mList, (void *)pm);
            freeNode((union twdNode *)pm);
            epicsMutexUnlock(mLock);
            return;
        }
        pm = (struct mNode *)ellNext(&pm->node);
    }
    epicsMutexUnlock(mLock);

    errlogPrintf("taskwdMonitorDel: Unregistered!\n");
}


/* Support old API for backwards compatibility */

static void anyNotify(void *usr, epicsThreadId tid, int suspended)
{
    struct aNode *pa = (struct aNode *)usr;

    if (suspended) {
        pa->callback(pa->usr, tid);
    }
}

static taskwdMonitor anyFuncs = {
    NULL, anyNotify, NULL
};

void taskwdAnyInsert(void *key, TASKWDANYFUNC callback, void *usr)
{
    struct mNode *pm;
    struct aNode *pa;

    if (callback == NULL) return;

    taskwdInit();

    pa = &allocNode()->a;
    pa->key = key;
    pa->callback = callback;
    pa->usr = usr;

    pm = &allocNode()->m;
    pm->funcs = &anyFuncs;
    pm->usr = pa;

    epicsMutexMustLock(mLock);
    ellAdd(&mList, (void *)pm);
    epicsMutexUnlock(mLock);
}

void taskwdAnyRemove(void *key)
{
    struct mNode *pm;
    struct aNode *pa;

    taskwdInit();

    epicsMutexMustLock(mLock);
    pm = (struct mNode *)ellFirst(&mList);
    while (pm) {
        if (pm->funcs == &anyFuncs) {
            pa = (struct aNode *)pm->usr;
            if (pa->key == key) {
                ellDelete(&mList, (void *)pm);
                freeNode((union twdNode *)pa);
                freeNode((union twdNode *)pm);
                epicsMutexUnlock(mLock);
                return;
            }
        }
        pm = (struct mNode *)ellNext(&pm->node);
    }
    epicsMutexUnlock(mLock);

    errlogPrintf("taskwdAnyRemove: Unregistered key %p\n", key);
}


/* Report function */

epicsShareFunc void taskwdShow(int level)
{
    struct tNode *pt;
    int mCount, fCount, tCount;
    char tName[40];

    epicsMutexMustLock(mLock);
    mCount = ellCount(&mList);
    epicsMutexUnlock(mLock);

    epicsMutexMustLock(fLock);
    fCount = ellCount(&fList);
    epicsMutexUnlock(fLock);

    epicsMutexMustLock(tLock);
    tCount = ellCount(&tList);
    printf("%d monitors, %d threads registered, %d free nodes\n",
        mCount, tCount, fCount);
    if (level) {
        printf("%16.16s %9s %12s %12s %12s\n",
            "THREAD NAME", "STATE", "EPICS TID", "CALLBACK", "USR ARG");
        pt = (struct tNode *)ellFirst(&tList);
        while (pt != NULL) {
            epicsThreadGetName(pt->tid, tName, sizeof(tName));
            printf("%16.16s %9s %12p %12p %12p\n",
                tName, pt->suspended ? "Suspended" : "Ok ",
                (void *)pt->tid, (void *)pt->callback, pt->usr);
            pt = (struct tNode *)ellNext(&pt->node);
        }
    }
    epicsMutexUnlock(tLock);
}


/* Free list management */

static union twdNode *newNode(void)
{
    union twdNode *pn;

    epicsMutexMustLock(fLock);
    pn = (union twdNode *)ellGet(&fList);
    if (pn) {
        VALGRIND_MEMPOOL_FREE(&fList, pn);
    }
    epicsMutexUnlock(fLock);
    if (!pn)
        pn = calloc(1, sizeof(union twdNode));
    if (pn)
        VALGRIND_MEMPOOL_ALLOC(&fList, pn, sizeof(*pn));
    return pn;
}

static union twdNode *allocNode(void)
{
    union twdNode *pn = newNode();
    while (!pn) {
        errlogPrintf("Thread taskwd suspending: out of memory\n");
        epicsThreadSuspendSelf();
        pn = newNode();
    }
    return pn;
}

static void freeNode(union twdNode *pn)
{
    VALGRIND_MEMPOOL_FREE(&fList, pn);
    VALGRIND_MEMPOOL_ALLOC(&fList, pn, sizeof(ELLNODE));
    epicsMutexMustLock(fLock);
    ellAdd(&fList, (void *)pn);
    epicsMutexUnlock(fLock);
}
