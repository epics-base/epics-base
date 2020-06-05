/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author: Andrew Johnson
 * Date:    May 21, 2008
 */

#include <stdio.h>
#include <string.h>

#include "taskwd.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* This is a unique prefix used for the names of the test threads */
#define baseName "testTask"

void monInsert(void *usr, epicsThreadId tid)
{
    char tname[32];

    epicsThreadGetName(tid, tname, sizeof(tname));
    if (strncmp(tname, baseName, strlen(baseName)) == 0)
        testPass("monInsert(thread='%s')", tname);
    else
        testDiag("monInsert(thread='%s')", tname);
}

void monNotify(void *usr, epicsThreadId tid, int suspended)
{
    char tname[32];

    epicsThreadGetName(tid, tname, sizeof(tname));
    testPass("monNotify(thread='%s', suspended=%d)", tname, suspended);
    epicsThreadResume(tid);
}

void monRemove(void *usr, epicsThreadId tid)
{
    char tname[32];

    epicsThreadGetName(tid, tname, sizeof(tname));
    if (strncmp(tname, baseName, strlen(baseName)) == 0)
        testPass("monRemove(thread='%s')", tname);
    else
        testDiag("monRemove(thread='%s')", tname);
}

taskwdMonitor monFuncs = {monInsert, monNotify, monRemove};

void anyNotify(void *usr, epicsThreadId tid)
{
    char tname[32];

    epicsThreadGetName(tid, tname, sizeof(tname));
    if (strncmp(tname, baseName, strlen(baseName)) == 0)
        testPass("anyNotify(thread='%s')", tname);
    else
        testDiag("anyNotify(thread='%s')", tname);
}

void taskNotify(void *usr)
{
    const char *id = (const char *) usr;

    testPass("taskNotify id='%s'", id);
}

void testTask1(void *arg)
{
    taskwdInsert(0, taskNotify, "1");
    epicsThreadSleep(14.0);
    testDiag("Task 1 cleaning up");
    taskwdRemove(0);
}

void testTask2(void *arg)
{
    taskwdInsert(0, taskNotify, "2");
    epicsThreadSleep(1.0);
    testDiag("Task 2 suspending");
    epicsThreadSuspendSelf();
    epicsThreadSleep(1.0);
    testDiag("Task 2 alive again");
    epicsThreadSleep(6.0);
    testDiag("Task 2 cleaning up");
    taskwdRemove(0);
}

MAIN(taskwdTest)
{
    eltc(0);
    testPlan(8);

    taskwdInit();
    taskwdMonitorAdd(&monFuncs, NULL);
    taskwdAnyInsert(NULL, anyNotify, NULL);

    epicsThreadCreate(baseName "1", epicsThreadPriorityMax,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        testTask1, NULL);
    epicsThreadSleep(1.0);

    epicsThreadCreate(baseName "2", epicsThreadPriorityMax,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        testTask2, NULL);

    /* taskwd checks tasks every 6 seconds */
    epicsThreadSleep(18.0);

    taskwdMonitorDel(&monFuncs, NULL);
    taskwdAnyRemove(NULL);

    eltc(1);
    return testDone();
}
