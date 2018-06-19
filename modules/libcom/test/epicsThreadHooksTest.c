/*************************************************************************\
* Copyright (c) 2012 ITER Organization
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
*
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* epicsThreadHooksTest.c */

#include <stdio.h>

#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define MAX_THREADS 16
#define TEST_THREADS 8
#define NUM_HOOKS 4

static int order[MAX_THREADS][NUM_HOOKS];
static int cnt[MAX_THREADS];
static int mine[MAX_THREADS];
static int mapped[MAX_THREADS];
static epicsThreadId tid[MAX_THREADS];

static epicsMutexId tidLock;
static epicsEventId shutdown[TEST_THREADS];

static int newThreadIndex(epicsThreadId id)
{
    int i = 0;

    if (epicsMutexLock(tidLock))
        testAbort("newThreadIndex: Locking problem");

    while (i < MAX_THREADS && tid[i] != 0)
        i++;
    if (i < MAX_THREADS)
        tid[i] = id;
    else
        testAbort("newThreadIndex: Too many threads!");

    epicsMutexUnlock(tidLock);
    return i;
}

static int findThreadIndex(epicsThreadId id)
{
    int i = 0;

    while (i < MAX_THREADS && tid[i] != id)
        i++;
    return i;
}

static void atExitHook1 (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());

    if (no < MAX_THREADS)
        order[no][3] = cnt[no]++;
}

static void atExitHook2 (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());

    if (no < MAX_THREADS)
        order[no][2] = cnt[no]++;
}

static void startHook1 (epicsThreadId id)
{
    int no = newThreadIndex(id);

    if (no < MAX_THREADS)
        order[no][0] = cnt[no]++;
    epicsAtThreadExit(atExitHook1, NULL);
}

static void startHook2 (epicsThreadId id)
{
    int no = findThreadIndex(id);

    if (no < MAX_THREADS)
        order[no][1] = cnt[no]++;
    epicsAtThreadExit(atExitHook2, NULL);
}

static void my_thread (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());

    if (no < MAX_THREADS)
        mine[no] = 1;
    epicsEventMustWait((epicsEventId) arg);
}

static void mapper (epicsThreadId id)
{
    int no = findThreadIndex(id);

    if (no < MAX_THREADS)
        mapped[no]++;
}

MAIN(epicsThreadHooksTest)
{
    int i;
    int ok;

    testPlan(TEST_THREADS + 1);

    tidLock = epicsMutexMustCreate();

    if (epicsThreadHookAdd(startHook1))
        testAbort("startHook1 registration failed");
    if (epicsThreadHookAdd(startHook2))
        testAbort("startHook2 registration failed");

    for (i = 0; i < TEST_THREADS; i++) {
        char name[10];

        shutdown[i] = epicsEventCreate(epicsEventEmpty);
        sprintf(name, "t%d", (int) i);
        epicsThreadCreate(name, epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackMedium),
            my_thread, shutdown[i]);
    }
    epicsThreadSleep(1.0);

    epicsThreadMap(mapper);

    ok = 1;
    for (i = 0; i < MAX_THREADS; i++) {
        if (!mine[i]) continue;

        if (mapped[i] != 1) {
            ok = 0;
            testDiag("mapped[%d] = %d", i, mapped[i]);
        }
    }
    testOk(ok, "All my tasks covered once by epicsThreadMap");

    for (i = 0; i < TEST_THREADS; i++) {
        epicsEventSignal(shutdown[i]);
    }

    epicsThreadSleep(1.0);

    for (i = 0; i < MAX_THREADS; i++) {
        int j;

        if (!mine[i]) continue;

        ok = 1;
        for (j = 0; j < NUM_HOOKS; j++) {
            if (order[i][j] != j) {
                ok = 0;
                testDiag("order[%d][%d] = %d", i, j, order[i][j]);
            }
        }
        testOk(ok, "All hooks for task %d called in correct order", i);
    }

    epicsThreadHookDelete(startHook1);
    epicsThreadHookDelete(startHook2);

    return testDone();
}
