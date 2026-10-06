/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* A queue of QSIZE entries with its one worker held inside a callback
 * accepts QSIZE-1 more requests and refuses the next with S_db_bufFull,
 * counting it in numOverflow. callbackQueueStatus() counts entries in
 * use, so numUsed reaches QSIZE and returns to 0 once the worker has
 * run everything and gone idle.
 */

#include <stdlib.h>

#include "callback.h"
#include "dbAccessDefs.h"
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define QSIZE 4

static epicsEventId holdStarted, holdRelease, shortDone;
static epicsCallback holdCb, shortCb[QSIZE - 1], oneMore;
static int nShortRun;

static void holdCallback(epicsCallback *pcb)
{
    epicsEventMustTrigger(holdStarted);
    epicsEventMustWait(holdRelease);
}

static void shortCallback(epicsCallback *pcb)
{
    if (epicsAtomicIncrIntT(&nShortRun) == QSIZE - 1)
        epicsEventMustTrigger(shortDone);
}

static int waitUsed(int want)
{
    callbackQueueStats st;
    int i;

    for (i = 0; i < 500; i++) {
        callbackQueueStatus(0, &st);
        if (st.numUsed[priorityLow] == want) return 1;
        epicsThreadSleep(0.01);
    }
    return 0;
}

MAIN(callbackOverflowTest)
{
    callbackQueueStats st;
    int i, n;

    testPlan(10);

    callbackSetQueueSize(QSIZE);
    callbackInit();

    holdStarted = epicsEventMustCreate(epicsEventEmpty);
    holdRelease = epicsEventMustCreate(epicsEventEmpty);
    shortDone = epicsEventMustCreate(epicsEventEmpty);
    callbackSetCallback(holdCallback, &holdCb);
    callbackSetPriority(priorityLow, &holdCb);
    for (i = 0; i < QSIZE - 1; i++) {
        callbackSetCallback(shortCallback, &shortCb[i]);
        callbackSetPriority(priorityLow, &shortCb[i]);
    }
    callbackSetCallback(shortCallback, &oneMore);
    callbackSetPriority(priorityLow, &oneMore);

    callbackRequest(&holdCb);
    epicsEventMustWait(holdStarted);

    n = 0;
    for (i = 0; i < QSIZE - 1; i++)
        if (callbackRequest(&shortCb[i]) == 0) n++;
    testOk(n == QSIZE - 1, "%d of %d requests behind the held worker accepted", n, QSIZE - 1);
    testOk(callbackRequest(&oneMore) == S_db_bufFull, "request %d refused with S_db_bufFull", QSIZE + 1);

    callbackQueueStatus(0, &st);
    testOk(st.size == QSIZE, "size %d", st.size);
    testOk(st.numUsed[priorityLow] == QSIZE, "numUsed %d while full", st.numUsed[priorityLow]);
    testOk(st.maxUsed[priorityLow] == QSIZE, "maxUsed %d", st.maxUsed[priorityLow]);
    testOk(st.numOverflow[priorityLow] == 1, "numOverflow %d", st.numOverflow[priorityLow]);
    testOk(st.numUsed[priorityMedium] == 0 && st.numUsed[priorityHigh] == 0,
        "other priorities unused");

    epicsEventMustTrigger(holdRelease);
    n = epicsEventWaitWithTimeout(shortDone, 5.0) == epicsEventOK;
    testOk(n, "%d short callbacks ran", epicsAtomicGetIntT(&nShortRun));
    testOk(waitUsed(0), "numUsed back to 0 once the worker is idle");

    callbackQueueStatus(1, &st);
    callbackQueueStatus(0, &st);
    testOk(st.maxUsed[priorityLow] == 0 && st.numOverflow[priorityLow] == 1,
        "reset clears maxUsed (%d) and keeps numOverflow (%d)",
        st.maxUsed[priorityLow], st.numOverflow[priorityLow]);

    callbackStop();
    callbackCleanup();
    epicsEventDestroy(holdStarted);
    epicsEventDestroy(holdRelease);
    epicsEventDestroy(shortDone);

    return testDone();
}
