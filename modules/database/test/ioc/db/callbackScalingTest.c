/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Callback queue throughput with 1, 2 and 4 parallel callback threads */

#include <stdlib.h>

#if !defined(_WIN32) && !defined(vxWorks)
#  include <pthread.h>
#  include <sched.h>
#endif

#include "callback.h"
#include "cantProceed.h"
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define NCALLBACKS 1000
#define NROUNDS 100
#define NWARMUP 10
#define ROUND_TIMEOUT 5.0
#define MAX_SLOWDOWN 10.0

static const int nWorkers[] = {1, 2, 4};
#define NCONFIGS (sizeof(nWorkers)/sizeof(nWorkers[0]))

static epicsCallback callbacks[NCALLBACKS];
static int count;
static epicsEventId roundDone;

static void countCallback(epicsCallback *pcallback)
{
    (void)pcallback;
    if (epicsAtomicIncrIntT(&count) == NCALLBACKS)
        epicsEventMustTrigger(roundDone);
}

static int workersRealtime;

static void schedCallback(epicsCallback *pcallback)
{
#if defined(_WIN32)
    workersRealtime = 0;
#elif defined(vxWorks)
    workersRealtime = 1;
#else
    int policy;
    struct sched_param param;

    workersRealtime = pthread_getschedparam(pthread_self(), &policy, &param) == 0 &&
                      (policy == SCHED_FIFO || policy == SCHED_RR);
#endif
    (void)pcallback;
    epicsEventMustTrigger(roundDone);
}

/* Returns the mean round time in seconds, or a negative value on failure */
static double runConfig(int workers)
{
    double sum = 0.0;
    int round, i;

    callbackParallelThreads(workers, "");
    callbackInit();

    callbackSetCallback(schedCallback, &callbacks[0]);
    callbackSetPriority(priorityLow, &callbacks[0]);
    callbackRequest(&callbacks[0]);
    epicsEventMustWait(roundDone);

    for (i = 0; i < NCALLBACKS; i++) {
        callbackSetCallback(countCallback, &callbacks[i]);
        callbackSetPriority(priorityLow, &callbacks[i]);
    }

    for (round = -NWARMUP; round < NROUNDS; round++) {
        epicsTimeStamp start, end;

        epicsAtomicSetIntT(&count, 0);
        epicsTimeGetCurrent(&start);
        for (i = 0; i < NCALLBACKS; i++) {
            if (callbackRequest(&callbacks[i])) {
                testDiag("callbackRequest() failed in round %d", round);
                sum = -1.0;
                goto done;
            }
        }
        if (epicsEventWaitWithTimeout(roundDone, ROUND_TIMEOUT) != epicsEventOK) {
            testDiag("round %d timed out, %d of %d callbacks ran",
                     round, epicsAtomicGetIntT(&count), NCALLBACKS);
            sum = -1.0;
            goto done;
        }
        epicsTimeGetCurrent(&end);
        if (round >= 0)
            sum += epicsTimeDiffInSeconds(&end, &start);
    }
    sum /= NROUNDS;

done:
    callbackStop();
    callbackCleanup();
    return sum;
}

MAIN(callbackScalingTest)
{
    double mean[NCONFIGS];
    unsigned i;

    testPlan(2 * NCONFIGS - 1);

    roundDone = epicsEventMustCreate(epicsEventEmpty);

    for (i = 0; i < NCONFIGS; i++) {
        mean[i] = runConfig(nWorkers[i]);
        testOk(mean[i] >= 0.0, "%d worker(s): all %d callbacks ran in each of %d rounds",
               nWorkers[i], NCALLBACKS, NROUNDS);
        if (mean[i] >= 0.0)
            testDiag("%d worker(s): %.1f us per round", nWorkers[i], mean[i] * 1e6);
    }

    testDiag("Callback workers use %sreal-time scheduling",
             workersRealtime ? "" : "no ");
    if (workersRealtime)
        testTodoBegin("priority inheritance on the queue lock serializes the workers");

    for (i = 1; i < NCONFIGS; i++) {
        double slowdown = mean[i] / mean[0];

        if (mean[0] <= 0.0 || mean[i] < 0.0) {
            testSkip(1, "missing measurement");
        } else if (testImpreciseTiming()) {
            testDiag("%d workers: %.1f times the time of 1 worker",
                     nWorkers[i], slowdown);
            testSkip(1, "imprecise timing");
        } else if (epicsThreadGetCPUs() < 2) {
            testSkip(1, "single CPU");
        } else {
            testOk(slowdown < MAX_SLOWDOWN,
                   "%d workers: %.1f times the time of 1 worker (limit %.0f)",
                   nWorkers[i], slowdown, MAX_SLOWDOWN);
        }
    }

    testTodoEnd();

    epicsEventDestroy(roundDone);

    return testDone();
}
