/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* ringPointerTest.c */

/* Author:  Marty Kraimer Date:    13OCT2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsRingPointer.h"
#include "errlog.h"
#include "epicsEvent.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define ringSize 10
#define consumerCount 4
#define producerCount 4

static volatile int testExit = 0;
int value[ringSize*2];

typedef struct info {
    epicsEventId consumerEvent;
    epicsRingPointerId	ring;
    int checkOrder;
    int value[ringSize*2];
}info;

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    static int expectedValue=0;
    int *newvalue;
    char myname[20];

    epicsThreadGetName(epicsThreadGetIdSelf(), myname, sizeof(myname));
    testDiag("%s starting", myname);
    while(1) {
        epicsEventMustWait(pinfo->consumerEvent);
        if (testExit) return;
        while ((newvalue = (int *)epicsRingPointerPop(pinfo->ring))) {
            if (pinfo->checkOrder) {
                testOk(expectedValue == *newvalue,
                    "%s: (got) %d == %d (expected)", myname, *newvalue, expectedValue);
                expectedValue = *newvalue + 1;
            } else {
                testOk(pinfo->value[*newvalue] <= producerCount, "%s: got a %d (%d times seen before)",
                        myname, *newvalue, pinfo->value[*newvalue]);
            }
            /* This must be atomic... */
            pinfo->value[*newvalue]++;
            epicsThreadSleep(0.05);
        }
    }
}

static void producer(void *arg)
{
    info *pinfo = (info *)arg;
    char myname[20];
    int i;

    epicsThreadGetName(epicsThreadGetIdSelf(), myname, sizeof(myname));
    testDiag("%s starting", myname);
    for (i=0; i<ringSize*2; i++) {
        while (epicsRingPointerIsFull(pinfo->ring)) {
            epicsThreadSleep(0.2);
            if (testExit) return;
        }
        testOk(epicsRingPointerPush(pinfo->ring, (void *)&value[i]),
               "%s: Pushing %d, ring not full", myname, i);
        epicsEventSignal(pinfo->consumerEvent);
        if (testExit) return;
    }
}

MAIN(ringPointerTest)
{
    int i;
    info *pinfo;
    epicsEventId consumerEvent;
    int *pgetValue;
    epicsRingPointerId ring;
    epicsThreadId tid;
    char threadName[20];

    testPlan(256);

    for (i=0; i<ringSize*2; i++) value[i] = i;

    pinfo = calloc(1,sizeof(info));
    if(!pinfo) testAbort("calloc failed");

    pinfo->consumerEvent = consumerEvent = epicsEventMustCreate(epicsEventEmpty);
    if (!consumerEvent) {
        testAbort("epicsEventMustCreate failed");
    }

    testDiag("******************************************************");
    testDiag("** Test 1: local ring pointer, check size and order **");
    testDiag("******************************************************");

    pinfo->ring = ring = epicsRingPointerCreate(ringSize);
    if (!ring) {
        testAbort("epicsRingPointerCreate failed");
    }
    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    for (i=0; epicsRingPointerPush(ring,(void *)&value[i]); ++i) {}
    testOk(i==ringSize, "ring filled, %d values", i);

    for (i=0; (pgetValue = (int *)epicsRingPointerPop(ring)); ++i) {
        testOk(i==*pgetValue, "Pop test: %d == %d", i, *pgetValue);
    }
    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    testDiag("**************************************************************");
    testDiag("** Test 2: unlocked ring pointer, one consumer, check order **");
    testDiag("**************************************************************");

    pinfo->checkOrder = 1;
    tid=epicsThreadCreate("consumer", 50, 
        epicsThreadGetStackSize(epicsThreadStackSmall), consumer, pinfo);
    if(!tid) testAbort("epicsThreadCreate failed");
    epicsThreadSleep(0.2);

    for (i=0; i<ringSize*2; i++) {
        if (epicsRingPointerIsFull(ring)) {
            epicsThreadSleep(0.2);
        }
        testOk(epicsRingPointerPush(ring, (void *)&value[i]), "Pushing %d, ring not full", i);
        epicsEventSignal(consumerEvent);
    }
    epicsThreadSleep(1.0);
    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    for (i=0; i<ringSize*2; i++) {
        testOk(pinfo->value[i] == 1, "Value test: %d was processed", i);
    }

    testExit = 1;
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(1.0);

    epicsRingPointerDelete(pinfo->ring);

    testDiag("*************************************************************************************");
    testDiag("** Test 3: locked ring pointer, many consumers, many producers, check no of copies **");
    testDiag("*************************************************************************************");

    pinfo->ring = ring = epicsRingPointerLockedCreate(ringSize);
    if (!ring) {
        testAbort("epicsRingPointerLockedCreate failed");
    }
    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    for (i=0; i<ringSize*2; i++) pinfo->value[i] = 0;
    testExit = 0;
    pinfo->checkOrder = 0;
    for (i=0; i<consumerCount; i++) {
        sprintf(threadName, "consumer%d", i);
        tid=epicsThreadCreate(threadName, 50,
                              epicsThreadGetStackSize(epicsThreadStackSmall), consumer, pinfo);
        if(!tid) testAbort("epicsThreadCreate failed");
    }
    epicsThreadSleep(0.2);

    for (i=0; i<producerCount; i++) {
        sprintf(threadName, "producer%d", i);
        tid=epicsThreadCreate(threadName, 50,
                              epicsThreadGetStackSize(epicsThreadStackSmall), producer, pinfo);
        if(!tid) testAbort("epicsThreadCreate failed");
    }

    epicsThreadSleep(0.5);
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(1.0);

    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    for (i=0; i<ringSize*2; i++) {
        testOk(pinfo->value[i] == producerCount, "Value test: %d was processed %d times", i, producerCount);
    }

    testExit = 1;
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(1.0);

    return testDone();
}
