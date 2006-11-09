/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsdThreadPriorityTest.cpp */

/* Author:  Marty Kraimer Date:    21NOV2005 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsUnitTest.h"
#include "testMain.h"


typedef struct info {
    epicsEventId      waitForMaster;
    epicsEventId      waitForClient;
}info;

extern "C" {

static void client(void *arg)
{
    info *pinfo = (info *)arg;
    epicsThreadId idSelf = epicsThreadGetIdSelf();
    int pass;

    for (pass = 0 ; pass < 3 ; pass++) {
        epicsEventWaitStatus status;
        status = epicsEventWait(pinfo->waitForMaster);
        testOk(status == epicsEventWaitOK,
            "task %p epicsEventWait returned %d", idSelf, status);
        epicsThreadSleep(0.01);
        epicsEventSignal(pinfo->waitForClient);
    }
}

static void runThreadPriorityTest(void *arg)
{
    epicsEventId testComplete = (epicsEventId) arg;
    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);

    epicsThreadId myId = epicsThreadGetIdSelf();
    epicsThreadSetPriority(myId, 50);

    info *pinfo = (info *)calloc(1, sizeof(info));
    pinfo->waitForMaster = epicsEventMustCreate(epicsEventEmpty);
    pinfo->waitForClient = epicsEventMustCreate(epicsEventEmpty);

    epicsThreadId clientId = epicsThreadCreate("client",
        50, stackSize, client, pinfo);
    epicsEventSignal(pinfo->waitForMaster);

    epicsEventWaitStatus status;
    status = epicsEventWaitWithTimeout(pinfo->waitForClient, 0.1);
    if (!testOk(status == epicsEventWaitOK,
        "epicsEventWaitWithTimeout returned %d", status)) {
        testSkip(2, "epicsEventWaitWithTimeout failed");
        goto done;
    }

    epicsThreadSetPriority(clientId, 20);
    /* expect that client will not be able to run */
    epicsEventSignal(pinfo->waitForMaster);

    status = epicsEventTryWait(pinfo->waitForClient);
    if (status != epicsEventWaitTimeout) {
        testFail("epicsEventTryWait returned %d", status);
    } else {
         status = epicsEventWaitWithTimeout(pinfo->waitForClient, 0.1);
         if (!testOk(status == epicsEventWaitOK,
             "epicsEventWaitWithTimeout returned %d", status)) {
             testSkip(1, "epicsEventWaitWithTimeout failed");
             goto done;
         }
    }
    epicsThreadSetPriority(clientId, 80);
    /* expect that client will be able to run */
    epicsEventSignal(pinfo->waitForMaster);
    status = epicsEventTryWait(pinfo->waitForClient);
    if (status==epicsEventWaitOK) {
        testPass("Strict priority scheduler");
    } else {
        testDiag("No strict priority scheduler");
        status = epicsEventWaitWithTimeout(pinfo->waitForClient,.1);
        testOk(status == epicsEventWaitOK,
            "epicsEventWaitWithTimeout returned %d", status);
    }
done:
    epicsThreadSleep(1.0);
    epicsEventSignal(testComplete);
}

} /* extern "C" */


MAIN(epicsThreadPriorityTest)
{
    testPlan(7);
    epicsEventId testComplete = epicsEventMustCreate(epicsEventEmpty);
    epicsThreadMustCreate("threadPriorityTest", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        runThreadPriorityTest, testComplete);
    epicsEventWaitStatus status = epicsEventWait(testComplete);
    testOk(status == epicsEventWaitOK,
        "epicsEventWait returned %d", status);
    return testDone();
}
