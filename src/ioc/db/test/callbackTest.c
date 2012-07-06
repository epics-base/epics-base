/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$ */

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

/*
 * This test checks both immediate and delayed callbacks in two steps.
 * In the first step (pass1) NCALLBACKS immediate callbacks are queued.
 * As each is run it starts a second delayed callback (pass2).
 * The last delayed callback which runs signals an epicsEvent
 * to the main thread.
 *
 * Two time intervals are measured.  The time to queue and run each of
 * the immediate callbacks, and the actual delay of the delayed callback.
 */

#define NCALLBACKS 169
#define DELAY_QUANTUM 0.25

#define TEST_DELAY(i) ((i / NUM_CALLBACK_PRIORITIES) * DELAY_QUANTUM)

typedef struct myPvt {
    CALLBACK cb1;
    CALLBACK cb2;
    epicsTimeStamp pass1Time;
    epicsTimeStamp pass2Time;
    double delay;
    int pass;
    int resultFail;
} myPvt;

epicsEventId finished;


static void myCallback(CALLBACK *pCallback)
{
    myPvt *pmyPvt;

    callbackGetUser(pmyPvt, pCallback);
    
    pmyPvt->pass++;

    if (pmyPvt->pass == 1) {
        epicsTimeGetCurrent(&pmyPvt->pass1Time);
        callbackRequestDelayed(&pmyPvt->cb2, pmyPvt->delay);
    } else if (pmyPvt->pass == 2) {
        epicsTimeGetCurrent(&pmyPvt->pass2Time);
    } else {
        pmyPvt->resultFail = 1;
        return;
    }
}

static void finalCallback(CALLBACK *pCallback)
{
    myCallback(pCallback);
    epicsEventSignal(finished);
}

MAIN(callbackTest)
{
    myPvt *pcbt[NCALLBACKS];
    epicsTimeStamp start;
    int i;

    testPlan(NCALLBACKS * 2 + 1);

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

    /* Last callback is special */
    callbackSetCallback(finalCallback, &pcbt[NCALLBACKS-1]->cb2);
    callbackSetPriority(0, &pcbt[NCALLBACKS-1]->cb1);
    callbackSetPriority(0, &pcbt[NCALLBACKS-1]->cb2);
    pcbt[NCALLBACKS-1]->delay = TEST_DELAY(NCALLBACKS) + 1.0;
    pcbt[NCALLBACKS-1]->pass = 0;

    testOk1(epicsTimeGetCurrent(&start)==epicsTimeOK);

    for (i = 0; i < NCALLBACKS ; i++) {
        callbackRequest(&pcbt[i]->cb1);
    }

    testDiag("Waiting %.02f sec", pcbt[NCALLBACKS-1]->delay);

    epicsEventWait(finished);
    
    for (i = 0; i < NCALLBACKS ; i++) {
        if(pcbt[i]->resultFail || pcbt[i]->pass!=2)
            testFail("pass = %d for delay = %f", pcbt[i]->pass, pcbt[i]->delay);
        else {
            double delta = epicsTimeDiffInSeconds(&pcbt[i]->pass1Time, &start);
            testOk(fabs(delta) < 0.05, "callback %.02f setup time |%f| < 0.05",
                    pcbt[i]->delay, delta);
        }
        
    }

    for (i = 0; i < NCALLBACKS ; i++) {
        double delta, error;
        if(pcbt[i]->resultFail || pcbt[i]->pass!=2)
            continue;
        delta = epicsTimeDiffInSeconds(&pcbt[i]->pass2Time, &pcbt[i]->pass1Time);
        error = delta - pcbt[i]->delay;
        testOk(fabs(error) < 0.05, "delay %.02f seconds, callback time error |%.04f| < 0.05",
                pcbt[i]->delay, error);
    }

    return testDone();
}
