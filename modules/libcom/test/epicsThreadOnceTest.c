/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "errlog.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define NUM_ONCE_THREADS 8

epicsThreadOnceId onceFlag = EPICS_THREAD_ONCE_INIT;
epicsThreadOnceId twiceFlag = EPICS_THREAD_ONCE_INIT;
epicsMutexId lock;
epicsEventId go;
epicsEventId done;

int runCount  = 0;
int initCount = 0;
char initBy[20];
int doneCount = 0;

void onceInit(void *ctx)
{
    initCount++;
    strcpy(initBy, epicsThreadGetNameSelf());
}

void onceThread(void *ctx)
{
    epicsMutexMustLock(lock);
    runCount++;
    epicsMutexUnlock(lock);

    epicsEventMustWait(go);
    epicsEventSignal(go);

    epicsThreadOnce(&onceFlag, onceInit, ctx);
    testOk(initCount == 1, "%s: initCount = %d",
        epicsThreadGetNameSelf(), initCount);

    epicsMutexMustLock(lock);
    doneCount++;
    if (doneCount == runCount)
        epicsEventSignal(done);
    epicsMutexUnlock(lock);
}

int getRunCount(void)
{
    int count;

    epicsMutexMustLock(lock);
    count = runCount;
    epicsMutexUnlock(lock);

    return count;
}


void recurseInit(void);
void onceRecurse(void *ctx)
{
    recurseInit();
}

void recurseInit(void)
{
    epicsThreadOnce(&twiceFlag, onceRecurse, 0);
}

void recurseThread(void *ctx)
{
    recurseInit();
    testFail("Recursive epicsThreadOnce() not detected");
}


MAIN(epicsThreadOnceTest)
{
    int i, count;
    epicsThreadId tid;

    testPlan(2 + NUM_ONCE_THREADS);

    go = epicsEventMustCreate(epicsEventEmpty);
    done = epicsEventMustCreate(epicsEventEmpty);
    lock = epicsMutexMustCreate();

    for (i = 0; i < NUM_ONCE_THREADS; i++) {
        char name[20];

        sprintf(name, "once-%d", i);
        epicsThreadCreate(name, epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackSmall),
            onceThread, 0);
    }
    while ((count = getRunCount()) < NUM_ONCE_THREADS)
        epicsThreadSleep(0.1);

    epicsEventSignal(go); /* Use epicsEventBroadcast(go) when available */
    epicsEventMustWait(done);

    epicsMutexMustLock(lock);
    count = doneCount;
    epicsMutexUnlock(lock);

    testOk(count == NUM_ONCE_THREADS, "doneCount = %d", count);
    testDiag("init was run by %s", initBy);

    eltc(0);
    tid = epicsThreadCreate("recurse", epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackSmall),
            recurseThread, 0);
    do {
        epicsThreadSleep(0.1);
    } while (!epicsThreadIsSuspended(tid));
    testPass("Recursive epicsThreadOnce() detected");

    eltc(1);
    return testDone();
}
