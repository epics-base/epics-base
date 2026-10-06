/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>

#include "epicsMPMCQueue.h"
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define NPROD 4
#define NCONS 4
#define NITEMS 20000
#define QSIZE 64

static void testSingleThread(void)
{
    static int items[8];
    int i, ok;
    epicsMPMCQueueId q;

    testOk1(epicsMPMCQueueCreate(0) == NULL);
    testOk1(epicsMPMCQueueCreate(-1) == NULL);
    testOk1(epicsMPMCQueueCreate((1 << 30) + 1) == NULL);
    epicsMPMCQueueDelete(NULL);

    q = epicsMPMCQueueCreate(1);
    testOk1(epicsMPMCQueueGetSize(q) == 2);
    testOk1(epicsMPMCQueuePush(q, &items[0]) && epicsMPMCQueuePush(q, &items[1]));
    testOk1(!epicsMPMCQueuePush(q, &items[2]));
    testOk1(epicsMPMCQueuePop(q) == &items[0] && epicsMPMCQueuePop(q) == &items[1]);
    epicsMPMCQueueDelete(q);

    q = epicsMPMCQueueCreate(5);
    testOk1(q != NULL);
    testOk1(epicsMPMCQueueGetSize(q) == 8);
    testOk1(epicsMPMCQueueIsEmpty(q));
    testOk1(epicsMPMCQueuePop(q) == NULL);
    testOk1(epicsMPMCQueuePush(q, NULL) == 0);

    for (i = 0, ok = 1; i < 8; i++)
        ok &= epicsMPMCQueuePush(q, &items[i]);
    testOk(ok, "8 pushes succeed");
    testOk1(epicsMPMCQueuePush(q, &items[0]) == 0);
    testOk1(!epicsMPMCQueueIsEmpty(q));
    testOk1(epicsMPMCQueueGetUsed(q) == 8);
    testOk1(epicsMPMCQueueGetHighWaterMark(q) == 8);

    for (i = 0, ok = 1; i < 8; i++)
        ok &= epicsMPMCQueuePop(q) == &items[i];
    testOk(ok, "8 pops in order");
    testOk1(epicsMPMCQueueIsEmpty(q));
    testOk1(epicsMPMCQueueGetUsed(q) == 0);

    for (i = 0, ok = 1; i < 1000; i++) {
        ok &= epicsMPMCQueuePush(q, &items[i % 8]);
        ok &= epicsMPMCQueuePush(q, &items[(i + 1) % 8]);
        ok &= epicsMPMCQueuePop(q) == &items[i % 8];
        ok &= epicsMPMCQueuePop(q) == &items[(i + 1) % 8];
    }
    testOk(ok, "FIFO order kept over many wrap-arounds of ring and position");

    testOk1(epicsMPMCQueueGetHighWaterMark(q) == 8);
    epicsMPMCQueueResetHighWaterMark(q);
    testOk1(epicsMPMCQueueGetHighWaterMark(q) == 0);

    epicsMPMCQueueDelete(q);
}

typedef struct {
    int prod;
    int seq;
} item;

static epicsMPMCQueueId stressQ;
static item items[NPROD][NITEMS];
static int received[NPROD][NITEMS];
static int nPopped;
static int orderErrors;
static epicsEventId done[NPROD + NCONS];

static void producer(void *arg)
{
    int p = (int)(size_t)arg, i;

    for (i = 0; i < NITEMS; i++) {
        while (!epicsMPMCQueuePush(stressQ, &items[p][i]))
            epicsThreadSleep(1e-6);
    }
    epicsEventMustTrigger(done[p]);
}

static void consumer(void *arg)
{
    int c = (int)(size_t)arg;
    int last[NPROD], p;

    for (p = 0; p < NPROD; p++)
        last[p] = -1;

    while (epicsAtomicGetIntT(&nPopped) < NPROD * NITEMS) {
        item *it = epicsMPMCQueuePop(stressQ);

        if (!it) {
            epicsThreadSleep(1e-6);
            continue;
        }
        if (it->seq <= last[it->prod])
            epicsAtomicIncrIntT(&orderErrors);
        last[it->prod] = it->seq;
        epicsAtomicIncrIntT(&received[it->prod][it->seq]);
        epicsAtomicIncrIntT(&nPopped);
    }
    epicsEventMustTrigger(done[NPROD + c]);
}

static void testMultiThread(void)
{
    int i, j, bad = 0;

    stressQ = epicsMPMCQueueCreate(QSIZE);
    for (i = 0; i < NPROD; i++)
        for (j = 0; j < NITEMS; j++) {
            items[i][j].prod = i;
            items[i][j].seq = j;
        }
    for (i = 0; i < NPROD + NCONS; i++)
        done[i] = epicsEventMustCreate(epicsEventEmpty);

    for (i = 0; i < NCONS; i++)
        epicsThreadMustCreate("consumer", epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackSmall),
            consumer, (void *)(size_t)i);
    for (i = 0; i < NPROD; i++)
        epicsThreadMustCreate("producer", epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackSmall),
            producer, (void *)(size_t)i);

    for (i = 0; i < NPROD + NCONS; i++) {
        epicsEventMustWait(done[i]);
        epicsEventDestroy(done[i]);
    }

    for (i = 0; i < NPROD; i++)
        for (j = 0; j < NITEMS; j++)
            if (received[i][j] != 1)
                bad++;
    testOk(bad == 0, "%d producers x %d items each received once (%d wrong)",
           NPROD, NITEMS, bad);
    testOk(orderErrors == 0, "each consumer saw each producer's items in order (%d errors)",
           orderErrors);
    testOk1(epicsMPMCQueueIsEmpty(stressQ));
    testOk1(epicsMPMCQueueGetHighWaterMark(stressQ) <= QSIZE);

    epicsMPMCQueueDelete(stressQ);
}

MAIN(epicsMPMCQueueTest)
{
    testPlan(27);
    testSingleThread();
    testMultiThread();
    return testDone();
}
