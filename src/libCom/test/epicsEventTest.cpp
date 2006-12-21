/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
        epicsEventWaitStatus status = epicsEventWait(pinfo->event);
        if (status != epicsEventWaitOK) {
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
        epicsEventSignal(pinfo->event);
    }
    testOk(errors == 0, "%s: errors = %d", name, errors);
}

} // extern "C"

static double eventWaitMeasureDelayError( const epicsEventId &id, const double & delay )
{
    epicsTime beg = epicsTime::getCurrent();
    epicsEventWaitWithTimeout ( id, delay );
    epicsTime end = epicsTime::getCurrent();
    double meas = end - beg;
    double error = fabs ( delay - meas );
    testDiag("epicsEventWaitWithTimeout(%.6f)  delay error %.6f sec",
        delay, error );
    return error;
}

static void eventWaitTest()
{
    double errorSum = 0.0;
    epicsEventId event = epicsEventMustCreate ( epicsEventEmpty );
    int i;
    for ( i = 0u; i < 20; i++ ) {
        double delay = ldexp ( 1.0 , -i );
        errorSum += eventWaitMeasureDelayError ( event, delay );
    }
    errorSum += eventWaitMeasureDelayError ( event, 0.0 );
    epicsEventDestroy ( event );
    double meanError = errorSum / ( i + 1 );
    testOk(meanError < 0.05, "Average error %.6f sec", meanError);
}


MAIN(epicsEventTest)
{
    const int nthreads = 3;
    epicsThreadId *id;
    char **name;
    epicsEventId event;
    int status;

    testPlan(10);

    event = epicsEventMustCreate(epicsEventEmpty);

    status = epicsEventWaitWithTimeout(event, 2.0);
    testOk(status == epicsEventWaitTimeout,
        "epicsEventWaitWithTimeout(event, 2.0) = %d", status);

    status = epicsEventTryWait(event);
    testOk(status == epicsEventWaitTimeout,
        "epicsEventTryWait(event) = %d", status);

    epicsEventSignal(event);
    status = epicsEventWaitWithTimeout(event,2.0);
    testOk(status == 0,
        "epicsEventWaitWithTimeout(event, 2.0) = %d", status);

    epicsEventSignal(event);
    status = epicsEventWaitWithTimeout(event,DBL_MAX);
    testOk(status == 0,
        "epicsEventWaitWithTimeout(event, DBL_MAX) = %d", status);

    epicsEventSignal(event);
    status = epicsEventTryWait(event);
    testOk(status == 0,
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
        name[i] = (char *)calloc(10, sizeof(char));
        sprintf(name[i],"producer %d",i);
        id[i] = epicsThreadCreate(name[i], 40, stackSize, producer, pinfo);
        epicsThreadSleep(0.1);
    }
    epicsThreadSleep(5.0);

    testDiag("setting quit");
    pinfo->quit = 1;
    epicsThreadSleep(2.0);

    epicsEventSignal(pinfo->event);
    epicsThreadSleep(1.0);

    eventWaitTest();

    return testDone();
}
