/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsEventTest.cpp */

/* Author:  Marty Kraimer Date:    26JAN2000 */
/*          timeout accuracy tests by Jeff Hill */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <float.h>

#include "epicsStdio.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsRingPointer.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"


typedef struct info {
    epicsEventId      event;
    epicsMutexId      lockRing;
    int               quit;
    epicsRingPointerId ring;
} info;

extern "C" {

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    int errors = 0;

    testDiag("consumer: starting");
    while (!pinfo->quit) {
        epicsEventStatus status = epicsEventWait(pinfo->event);
        if (status != epicsEventOK) {
            testDiag("consumer: epicsEventWait returned %d", status);
            errors++;
        }
        while (epicsRingPointerGetUsed(pinfo->ring) >= 2) {
            epicsRingPointerId message[2];
            for (int i = 0; i < 2; i++) {
                message[i] = epicsRingPointerPop(pinfo->ring);
                if (message[i] == 0) {
                    testDiag("consumer: epicsRingPointerPop returned 0");
                    errors++;
                }
            }
            if (message[0] != message[1]) {
                testDiag("consumer: message %p %p\n", message[0], message[1]);
                errors++;
            }
        }
    }
    testOk(errors == 0, "consumer: errors = %d", errors);
}

static void producer(void *arg)
{
    info *pinfo = (info *)arg;
    const char *name = epicsThreadGetNameSelf();
    epicsThreadId myId = epicsThreadGetIdSelf();
    int errors = 0;
    int ntimes = 0;

    testDiag("%s: starting", name);
    while(!pinfo->quit) {
        ++ntimes;
        epicsMutexLockStatus status = epicsMutexLock(pinfo->lockRing);
        if (status != epicsMutexLockOK) {
            testDiag("%s: epicsMutexLock returned %d", name, status);
            errors++;
        }
        if (epicsRingPointerGetFree(pinfo->ring) >= 2) {
            for (int i = 0; i < 2; i++) {
                if (!epicsRingPointerPush(pinfo->ring, myId)) {
                    testDiag("%s: epicsRingPointerPush fail", name);
                    errors++;
                }
                if (i == 0 && (ntimes % 4 == 0))
                    epicsThreadSleep(0.1);
            }
        } else {
           testFail("%s: ring buffer full", name);
           errors++;
        }
        epicsMutexUnlock(pinfo->lockRing);
        epicsThreadSleep(1.0);
        epicsEventMustTrigger(pinfo->event);
    }
    testOk(errors == 0, "%s: errors = %d", name, errors);
}

#define SLEEPERCOUNT 3
struct wakeInfo {
    epicsEventId event;
    epicsMutexId countMutex;
    int          count;
};
static void sleeper(void *arg)
{
    struct wakeInfo *wp = (struct wakeInfo *)arg;
    epicsEventMustWait(wp->event);
    epicsMutexLock(wp->countMutex);
    wp->count++;
    epicsMutexUnlock(wp->countMutex);
}
static void eventWakeupTest(void)
{
    struct wakeInfo wakeInfo, *wp = &wakeInfo;
    int i, c;

    wp->event = epicsEventMustCreate(epicsEventEmpty);
    wp->countMutex = epicsMutexMustCreate();
    wp->count = 0;
    for (i = 0 ; i < SLEEPERCOUNT ; i++)
        epicsThreadCreate("Sleeper",
                          epicsThreadPriorityScanHigh,
                          epicsThreadGetStackSize(epicsThreadStackSmall),
                          sleeper,
                          wp);
    epicsThreadSleep(0.5);
    epicsMutexLock(wp->countMutex);
    c = wp->count;
    epicsMutexUnlock(wp->countMutex);
    testOk(c == 0, "all threads still sleeping");
    for (i = 1 ; i <= SLEEPERCOUNT ; i++) {
        epicsEventMustTrigger(wp->event);
        epicsThreadSleep(0.5);
        epicsMutexLock(wp->countMutex);
        c = wp->count;
        epicsMutexUnlock(wp->countMutex);
        testOk(c == i, "%d thread%s awakened, expected %d", c, c == 1 ? "" : "s", i);
    }
    epicsEventDestroy(wp->event);
    epicsMutexDestroy(wp->countMutex);
}


} // extern "C"

static double eventWaitCheckDelayError( const epicsEventId &id, const double & delay )
{
    epicsEventWaitWithTimeout ( id, 0.000001 );

    epicsTime beg = epicsTime::getMonotonic();
    epicsEventWaitWithTimeout ( id, delay );
    epicsTime end = epicsTime::getMonotonic();
    double meas = end - beg;
    double error = meas - delay;
    testOk(error >= 0, "epicsEventWaitWithTimeout(%.6f)  delay error %.6f sec",
        delay, error);
    return fabs(error);
}

#define WAITCOUNT 21
static void eventWaitTest()
{
#if defined(_WIN32) || defined(__rtems__) || defined(vxWorks)
    testTodoBegin("Known issue with delay calculation");
#endif

    epicsEventId event = epicsEventMustCreate(epicsEventEmpty);
    double errorSum = eventWaitCheckDelayError(event, 0.0);

    for (int i = 0; i < WAITCOUNT - 1; i++) {
        double delay = ldexp ( 1.0 , -i );
        errorSum += eventWaitCheckDelayError ( event, delay );
    }
    double meanError = errorSum / WAITCOUNT;
    testOk(meanError < 0.05, "Mean delay error was %.6f sec", meanError);

    testTodoEnd();

    epicsEventDestroy(event);
}


/* Two threads hand control back and forth through two events. A lost
 * wake-up shows up as a timeout.
 */
#define PINGPONGROUNDS 100000
struct pingPongInfo {
    epicsEventId ping, pong;
    int timeouts;
};

static void ponger(void *arg)
{
    pingPongInfo *pp = (pingPongInfo *)arg;
    for (int i = 0; i < PINGPONGROUNDS; i++) {
        if (epicsEventWaitWithTimeout(pp->ping, 10.0) != epicsEventOK)
            pp->timeouts++;
        epicsEventMustTrigger(pp->pong);
    }
}

static void eventPingPongTest(void)
{
    pingPongInfo pp;
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    int timeouts = 0;

    opts.joinable = 1;
    opts.stackSize = epicsThreadStackSmall;
    pp.ping = epicsEventMustCreate(epicsEventEmpty);
    pp.pong = epicsEventMustCreate(epicsEventEmpty);
    pp.timeouts = 0;

    epicsTime beg = epicsTime::getMonotonic();
    epicsThreadId tid = epicsThreadCreateOpt("ponger", ponger, &pp, &opts);
    if (!tid)
        testAbort("Failed to create thread");
    for (int i = 0; i < PINGPONGROUNDS; i++) {
        epicsEventMustTrigger(pp.ping);
        if (epicsEventWaitWithTimeout(pp.pong, 10.0) != epicsEventOK)
            timeouts++;
    }
    epicsThreadMustJoin(tid);
    double elapsed = epicsTime::getMonotonic() - beg;

    testDiag("%d round trips in %.3f sec, %.2f usec each",
        PINGPONGROUNDS, elapsed, elapsed * 1e6 / PINGPONGROUNDS);
    testOk(timeouts == 0 && pp.timeouts == 0,
        "ping-pong without lost wake-ups (%d + %d timeouts)",
        timeouts, pp.timeouts);

    epicsEventDestroy(pp.ping);
    epicsEventDestroy(pp.pong);
}

/* Several threads trigger one event while several others wait on it.
 * No call may fail, a waiter can never take more than was triggered,
 * and once the triggering stops and the waiters are gone a single
 * trigger and wait must still work.
 */
#define STRESSWAITERS 4
#define STRESSTRIGGERERS 4
#define STRESSTRIGGERS 50000
struct stressInfo {
    epicsEventId event;
    epicsMutexId lock;
    int quit;
    unsigned long taken;
    unsigned long triggered;
    int errors;
};

static void stressWaiter(void *arg)
{
    stressInfo *si = (stressInfo *)arg;
    unsigned long taken = 0;
    int errors = 0;
    int quit;

    do {
        epicsEventStatus status = epicsEventWaitWithTimeout(si->event, 0.01);
        if (status == epicsEventOK)
            taken++;
        else if (status != epicsEventWaitTimeout)
            errors++;
        epicsMutexMustLock(si->lock);
        quit = si->quit;
        epicsMutexUnlock(si->lock);
    } while (!quit);

    epicsMutexMustLock(si->lock);
    si->taken += taken;
    si->errors += errors;
    epicsMutexUnlock(si->lock);
}

static void stressTriggerer(void *arg)
{
    stressInfo *si = (stressInfo *)arg;
    int errors = 0;

    for (int i = 0; i < STRESSTRIGGERS; i++) {
        if (epicsEventTrigger(si->event) != epicsEventOK)
            errors++;
        if (i % 64 == 0)
            epicsThreadSleep(0.0);
    }
    epicsMutexMustLock(si->lock);
    si->triggered += STRESSTRIGGERS;
    si->errors += errors;
    epicsMutexUnlock(si->lock);
}

static void eventStressTest(void)
{
    stressInfo si;
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    epicsThreadId waiters[STRESSWAITERS], triggerers[STRESSTRIGGERERS];

    opts.joinable = 1;
    opts.stackSize = epicsThreadStackSmall;
    si.event = epicsEventMustCreate(epicsEventEmpty);
    si.lock = epicsMutexMustCreate();
    si.quit = 0;
    si.taken = si.triggered = 0;
    si.errors = 0;

    for (int i = 0; i < STRESSWAITERS; i++) {
        waiters[i] = epicsThreadCreateOpt("waiter", stressWaiter, &si, &opts);
        if (!waiters[i])
            testAbort("Failed to create thread");
    }
    for (int i = 0; i < STRESSTRIGGERERS; i++) {
        triggerers[i] = epicsThreadCreateOpt("trigger", stressTriggerer, &si,
            &opts);
        if (!triggerers[i])
            testAbort("Failed to create thread");
    }
    for (int i = 0; i < STRESSTRIGGERERS; i++)
        epicsThreadMustJoin(triggerers[i]);
    epicsMutexMustLock(si.lock);
    si.quit = 1;
    epicsMutexUnlock(si.lock);
    for (int i = 0; i < STRESSWAITERS; i++)
        epicsThreadMustJoin(waiters[i]);

    /* the event may still be full from the last trigger */
    if (epicsEventTryWait(si.event) == epicsEventOK)
        si.taken++;

    testDiag("%lu triggers, %lu taken", si.triggered, si.taken);
    testOk(si.errors == 0 && si.taken >= 1 && si.taken <= si.triggered,
        "stress: %d errors, %lu taken of %lu triggers",
        si.errors, si.taken, si.triggered);

    epicsEventMustTrigger(si.event);
    testOk(epicsEventWaitWithTimeout(si.event, 1.0) == epicsEventOK &&
           epicsEventTryWait(si.event) == epicsEventWaitTimeout,
        "event still works after the stress test");

    epicsEventDestroy(si.event);
    epicsMutexDestroy(si.lock);
}

MAIN(epicsEventTest)
{
    const int nthreads = 3;
    epicsThreadId *id;
    char **name;
    epicsEventId event;
    int status;

    testPlan(16 + SLEEPERCOUNT + WAITCOUNT);

    event = epicsEventMustCreate(epicsEventEmpty);

    status = epicsEventWaitWithTimeout(event, 0.0);
    testOk(status == epicsEventWaitTimeout,
        "epicsEventWaitWithTimeout(event, 0.0) = %d", status);

    status = epicsEventWaitWithTimeout(event, 1.0);
    testOk(status == epicsEventWaitTimeout,
        "epicsEventWaitWithTimeout(event, 1.0) = %d", status);

    status = epicsEventTryWait(event);
    testOk(status == epicsEventWaitTimeout,
        "epicsEventTryWait(event) = %d", status);

    status = epicsEventTrigger(event);
    testOk(status == epicsEventOK,
        "epicsEventTrigger(event) = %d", status);
    status = epicsEventWaitWithTimeout(event, 1.0);
    testOk(status == epicsEventOK,
        "epicsEventWaitWithTimeout(event, 1.0) = %d", status);

    epicsEventMustTrigger(event);
    status = epicsEventWaitWithTimeout(event,DBL_MAX);
    testOk(status == epicsEventOK,
        "epicsEventWaitWithTimeout(event, DBL_MAX) = %d", status);

    epicsEventMustTrigger(event);
    status = epicsEventTryWait(event);
    testOk(status == epicsEventOK,
        "epicsEventTryWait(event) = %d", status);

    info *pinfo = (info *)calloc(1,sizeof(info));
    pinfo->event = event;
    pinfo->lockRing = epicsMutexCreate();
    pinfo->ring = epicsRingPointerCreate(1024*2);
    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);

    epicsThreadCreate("consumer", 50, stackSize, consumer, pinfo);
    id = (epicsThreadId *)calloc(nthreads, sizeof(epicsThreadId));
    name = (char **)calloc(nthreads, sizeof(char *));
    for(int i = 0; i < nthreads; i++) {
        name[i] = (char *)calloc(16, sizeof(char));
        epicsSnprintf(name[i], 16, "producer %d",i);
        id[i] = epicsThreadCreate(name[i], 40, stackSize, producer, pinfo);
        epicsThreadSleep(0.1);
    }
    epicsThreadSleep(5.0);

    testDiag("setting quit");
    pinfo->quit = 1;
    epicsThreadSleep(2.0);

    epicsEventMustTrigger(pinfo->event);
    epicsThreadSleep(1.0);

    eventWaitTest();
    eventWakeupTest();
    eventPingPongTest();
    eventStressTest();

    free(name);
    free(id);
    epicsRingPointerDelete(pinfo->ring);
    epicsMutexDestroy(pinfo->lockRing);
    epicsEventDestroy(event);
    free(pinfo);

    return testDone();
}
