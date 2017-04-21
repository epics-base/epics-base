/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author: Andrew Johnson
 * Date:    May 21, 2008
 */

#include <stdio.h>

#include "taskwd.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"


void monInsert(void *usr, epicsThreadId tid)
{
    testPass("monInsert(tid=%p)", (void *)tid);
}

void monNotify(void *usr, epicsThreadId tid, int suspended)
{
    testPass("monNotify(tid=%p, suspended=%d)", (void *)tid, suspended);
    epicsThreadResume(tid);
}

void monRemove(void *usr, epicsThreadId tid)
{
    testPass("monRemove(tid=%p)", (void *)tid);
}

taskwdMonitor monFuncs = {monInsert, monNotify, monRemove};

void anyNotify(void *usr, epicsThreadId tid)
{
    testPass("anyNotify(tid=%p)", (void *)tid);
}

void taskNotify(void *usr)
{
    testPass("taskNotify");
}

void testTask1(void *arg)
{
    taskwdInsert(0, taskNotify, NULL);
    epicsThreadSleep(10.0);
    taskwdRemove(0);
}

void testTask2(void *arg)
{
    taskwdInsert(0, taskNotify, NULL);
    testDiag("Task suspending");
    epicsThreadSuspendSelf();
    epicsThreadSleep(1.0);
    testDiag("Alive again");
    epicsThreadSleep(10.0);
    taskwdRemove(0);
}

MAIN(taskwdTest)
{
    eltc(0);
    testPlan(8);

    taskwdInit();
    taskwdMonitorAdd(&monFuncs, NULL);
    taskwdAnyInsert(NULL, anyNotify, NULL);

    epicsThreadCreate("testTask1", epicsThreadPriorityMax,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        testTask1, NULL);
    epicsThreadSleep(1.0);

    epicsThreadCreate("testTask2", epicsThreadPriorityMax,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        testTask2, NULL);

    /* taskwd checks tasks every 6 seconds */
    epicsThreadSleep(18.0);

    taskwdMonitorDel(&monFuncs, NULL);
    taskwdAnyRemove(NULL);

    eltc(1);
    return testDone();
}

