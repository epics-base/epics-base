/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

static volatile int testExit = 0;

typedef struct info {
    epicsEventId consumerEvent;
    epicsRingPointerId	ring;
}info;

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    static int expectedValue=0;
    int *newvalue;

    testDiag("Consumer starting");
    while(1) {
        epicsEventMustWait(pinfo->consumerEvent);
        if (testExit) return;
        while((newvalue = (int *)epicsRingPointerPop(pinfo->ring))) {
            testOk(expectedValue == *newvalue, 
                "Consumer: %d == %d", expectedValue, *newvalue);
            expectedValue = *newvalue + 1;
        }  
    }
}

MAIN(ringPointerTest)
{
    int i;
    info *pinfo;
    epicsEventId consumerEvent;
    int value[ringSize*2];
    int *pgetValue;
    epicsRingPointerId ring;
    epicsThreadId tid;

    testPlan(54);

    for (i=0; i<ringSize*2; i++) value[i] = i;
    pinfo = calloc(1,sizeof(info));
    if(!pinfo) testAbort("calloc failed");
    pinfo->consumerEvent = consumerEvent = epicsEventMustCreate(epicsEventEmpty);
    if (!consumerEvent) {
        testAbort("epicsEventMustCreate failed");
    }

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

    tid=epicsThreadCreate("consumer", 50, 
        epicsThreadGetStackSize(epicsThreadStackSmall), consumer, pinfo);
    if(!tid) testAbort("epicsThreadCreate failed");
    epicsThreadSleep(0.1);

    for (i=0; i<ringSize*2; i++) {
        if (epicsRingPointerIsFull(ring)) {
            epicsEventSignal(consumerEvent);
            epicsThreadSleep(0.2);
        }
        testOk(epicsRingPointerPush(ring, (void *)&value[i]), "Ring not full");
    }
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(0.2);
    testOk(epicsRingPointerIsEmpty(ring), "Ring empty");

    testExit = 1;
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(1.0);

    return testDone();
}
