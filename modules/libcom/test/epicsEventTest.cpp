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
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <float.h>

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

static double eventWaitCheckDelayError( const epicsEventId &id, const double & delay, int absWait )
{
    epicsEventWaitWithTimeout ( id, 0.000001 );

    epicsTime beg = epicsTime::getMonotonic();
    if ( absWait ) {
        epicsTimeStamp ts = epicsTime::getCurrent() + delay;
        epicsEventWaitWithAbsTimeout ( id, &ts );
    } else {
        epicsEventWaitWithTimeout ( id, delay );
    }
    epicsTime end = epicsTime::getMonotonic();
    double meas = end - beg;
    double error = meas - delay;
    testOk(error >= 0, "epicsEventWaitWith%sTimeout(%s%.6f)  delay error %.6f sec",
        absWait ? "Abs" : "", absWait ? "now + " : "", delay, error);
    return fabs(error);
}

#define WAITCOUNT 21
static void eventWaitTest(int absWait)
{
#if defined(_WIN32) || defined(__rtems__) || defined(vxWorks)
    testTodoBegin("Known issue with delay calculation");
#endif

    epicsEventId event = epicsEventMustCreate(epicsEventEmpty);
    double errorSum = eventWaitCheckDelayError(event, 0.0, absWait);

    for (int i = 0; i < WAITCOUNT - 1; i++) {
        double delay = ldexp ( 1.0 , -i );
        errorSum += eventWaitCheckDelayError ( event, delay, absWait );
    }
    double meanError = errorSum / WAITCOUNT;
    testOk(meanError < 0.05, "Mean delay error was %.6f sec", meanError);

    testTodoEnd();

    epicsEventDestroy(event);
}


MAIN(epicsEventTest)
{
    const int nthreads = 3;
    epicsThreadId *id;
    char **name;
    epicsEventId event;
    int status;

    testPlan(12 + SLEEPERCOUNT + (1+WAITCOUNT)*2);

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
        sprintf(name[i],"producer %d",i);
        id[i] = epicsThreadCreate(name[i], 40, stackSize, producer, pinfo);
        epicsThreadSleep(0.1);
    }
    epicsThreadSleep(5.0);

    testDiag("setting quit");
    pinfo->quit = 1;
    epicsThreadSleep(2.0);

    epicsEventMustTrigger(pinfo->event);
    epicsThreadSleep(1.0);

    eventWaitTest(0);
    eventWaitTest(1);
    eventWakeupTest();

    free(name);
    free(id);
    epicsRingPointerDelete(pinfo->ring);
    epicsMutexDestroy(pinfo->lockRing);
    epicsEventDestroy(event);
    free(pinfo);

    return testDone();
}
