/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Author:  Marty Kraimer Date:    26JAN2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "callback.h"
#include "cantProceed.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"
#include "testMain.h"


#define NCALLBACKS 168
#define DELAY_QUANTUM 0.25

#define TEST_DELAY(i) ((i / NUM_CALLBACK_PRIORITIES) * DELAY_QUANTUM)

typedef struct myPvt {
    CALLBACK cb1;
    CALLBACK cb2;
    epicsTimeStamp start;
    double delay;
    int pass;
} myPvt;

epicsEventId finished;


static void myCallback(CALLBACK *pCallback)
{
    myPvt *pmyPvt;
    epicsTimeStamp now;
    double delay, error;

    epicsTimeGetCurrent(&now);
    callbackGetUser(pmyPvt, pCallback);

    if (pmyPvt->pass++ == 0) {
        delay = 0.0;
        error = epicsTimeDiffInSeconds(&now, &pmyPvt->start);
        pmyPvt->start = now;
        callbackRequestDelayed(&pmyPvt->cb2, pmyPvt->delay);
    } else if (pmyPvt->pass == 2) {
        double diff = epicsTimeDiffInSeconds(&now, &pmyPvt->start);
        delay = pmyPvt->delay;
        error = fabs(delay - diff);
    } else {
        testFail("pass = %d for delay = %f", pmyPvt->pass, pmyPvt->delay);
        return;
    }
    testOk(error < 0.05, "delay %f seconds, callback time error %f",
        delay, error);
}

static void finalCallback(CALLBACK *pCallback)
{
    myCallback(pCallback);
    epicsEventSignal(finished);
}

MAIN(callbackTest)
{
    myPvt *pcbt[NCALLBACKS];
    myPvt *pfinal;
    int i;

    testPlan(NCALLBACKS * 2 + 2);

    callbackInit();
    epicsThreadSleep(1.0);

    finished = epicsEventMustCreate(epicsEventEmpty);

    for (i = 0; i < NCALLBACKS ; i++) {
        pcbt[i] = callocMustSucceed(1, sizeof(myPvt), "pcbt");
        callbackSetCallback(myCallback, &pcbt[i]->cb1);
        callbackSetCallback(myCallback, &pcbt[i]->cb2);
        callbackSetUser(pcbt[i], &pcbt[i]->cb1);
        callbackSetUser(pcbt[i], &pcbt[i]->cb2);
        callbackSetPriority(i % NUM_CALLBACK_PRIORITIES, &pcbt[i]->cb1);
        callbackSetPriority(i % NUM_CALLBACK_PRIORITIES, &pcbt[i]->cb2);
        pcbt[i]->delay = TEST_DELAY(i);
        pcbt[i]->pass = 0;
    }

    for (i = 0; i < NCALLBACKS ; i++) {
        epicsTimeGetCurrent(&pcbt[i]->start);
        callbackRequest(&pcbt[i]->cb1);
    }

    pfinal = callocMustSucceed(1, sizeof(myPvt), "final");
    callbackSetCallback(myCallback, &pfinal->cb1);
    callbackSetCallback(finalCallback, &pfinal->cb2);
    callbackSetUser(pfinal, &pfinal->cb1);
    callbackSetUser(pfinal, &pfinal->cb2);
    callbackSetPriority(0, &pfinal->cb1);
    callbackSetPriority(0, &pfinal->cb2);
    pfinal->delay = TEST_DELAY(NCALLBACKS) + 1.0;
    pfinal->pass = 0;

    epicsTimeGetCurrent(&pfinal->start);
    callbackRequest(&pfinal->cb1);

    epicsEventWait(finished);

    return testDone();
}
