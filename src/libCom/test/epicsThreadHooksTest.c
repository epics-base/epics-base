/*************************************************************************\
* Copyright (c) 2012 ITER Organization
*
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* epicsThreadHooksTest.c */

#include <stdio.h>
#include <stdint.h>

#include "epicsThread.h"
#include "epicsExit.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define THREAD_NO 6
#define HOOKS_NO 6

epicsThreadPrivateId threadNo;
static int order[THREAD_NO][HOOKS_NO];
static int cnt[THREAD_NO];
static int mine[THREAD_NO];
static epicsThreadId tid[THREAD_NO];

static int newThreadIndex(epicsThreadId id)
{
    int i = 0;
    while (tid[i] != 0 && i < THREAD_NO) i++;
    tid[i] = id;
    return i;
}

static int findThreadIndex(epicsThreadId id)
{
    int i = 0;
    while (tid[i] !=  id && i < THREAD_NO) i++;
    return i;
}

static void startHook1 (epicsThreadId id)
{
    int no = newThreadIndex(id);
    order[no][0] = cnt[no]++;
}

static void startHook2 (epicsThreadId id)
{
    int no = findThreadIndex(id);
    order[no][1] = cnt[no]++;
}

static void atExitHook1 (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());
    order[no][3] = cnt[no]++;
}

static void atExitHook2 (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());
    order[no][2] = cnt[no]++;
}

static void exitHook1 (epicsThreadId id)
{
    int no = findThreadIndex(id);
    order[no][5] = cnt[no]++;
}

static void exitHook2 (epicsThreadId id)
{
    int no = findThreadIndex(id);
    order[no][4] = cnt[no]++;
}

static void my_thread (void *arg)
{
    int no = findThreadIndex(epicsThreadGetIdSelf());
    mine[no] = 1;
    epicsAtThreadExit(atExitHook1, NULL);
    epicsAtThreadExit(atExitHook2, NULL);
    epicsThreadSleep(0.1);
}

MAIN(epicsThreadHooksTest)
{
    int i;

    testPlan(THREAD_NO-1);
    epicsThreadAddStartHook(startHook1);
    epicsThreadAddExitHook(exitHook1);
    epicsThreadAddStartHook(startHook2);
    epicsThreadAddExitHook(exitHook2);

    for (i = 0; i < THREAD_NO-1; i++) {
        char name[10];
        sprintf(name, "t%d", (int) i);
        epicsThreadCreate(name, epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), my_thread, NULL);
    }

    epicsThreadSleep(1.0);

    for (i = 0; i < THREAD_NO; i++) {
        int ok = 1;
        int j;
        for (j = 0; j < HOOKS_NO; j++) {
            if (mine[i] && order[i][j]!=j) ok = 0;
        }
        if (mine[i]) testOk(ok, "All %d hooks for task %d called in correct order", HOOKS_NO, (int)i);
    }
    return testDone();
}
