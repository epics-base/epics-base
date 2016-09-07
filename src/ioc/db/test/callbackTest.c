/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
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

static void updateStats(double *stats, double val)
{
    if (stats[0] > val) stats[0] = val;
    if (stats[1] < val) stats[1] = val;
    stats[2] += val;
    stats[3] += pow(val, 2.0);
    stats[4] += 1.;
}

static void printStats(double *stats, const char* tag) {
    testDiag("Priority %4s  min/avg/max/sigma = %f / %f / %f / %f",
            tag, stats[0], stats[2]/stats[4], stats[1],
            sqrt(stats[4]*stats[3]-pow(stats[2], 2.0))/stats[4]);
}

MAIN(callbackTest)
{
    myPvt *pcbt[NCALLBACKS];
    epicsTimeStamp start;
    int i, j, slowups, faults;
    /* Statistics: min/max/sum/sum^2/n for each priority */
    double setupError[NUM_CALLBACK_PRIORITIES][5];
    double timeError[NUM_CALLBACK_PRIORITIES][5];
    double defaultError[5] = {1,-1,0,0,0};

    for (i = 0; i < NUM_CALLBACK_PRIORITIES; i++)
        for (j = 0; j < 5; j++)
            setupError[i][j] = timeError[i][j] = defaultError[j];

    testPlan(4);

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

    testOk(epicsTimeGetCurrent(&start)==epicsTimeOK, "Time-of-day clock Ok");

    for (i = 0; i < NCALLBACKS ; i++) {
        callbackRequest(&pcbt[i]->cb1);
    }

    testDiag("Waiting %.02f sec", pcbt[NCALLBACKS-1]->delay);

    epicsEventWait(finished);
    slowups = 0;
    faults = 0;

    for (i = 0; i < NCALLBACKS ; i++) {
        if(pcbt[i]->resultFail || pcbt[i]->pass!=2)
            testDiag("callback setup fault #%d: pass = %d for delay = %.02f",
                ++faults, pcbt[i]->pass, pcbt[i]->delay);
        else {
            double delta = epicsTimeDiffInSeconds(&pcbt[i]->pass1Time, &start);

            if (fabs(delta) >= 0.05) {
                slowups++;
                testDiag("callback %.02f setup time |%f| >= 0.05 seconds",
                    pcbt[i]->delay, delta);
            }
            updateStats(setupError[i%NUM_CALLBACK_PRIORITIES], delta);
        }
    }
    testOk(faults == 0, "%d faults during callback setup", faults);
    testOk(slowups <= 1, "%d slowups during callback setup", slowups);

    slowups = 0;
    for (i = 0; i < NCALLBACKS ; i++) {
        double delta, error;

        if(pcbt[i]->resultFail || pcbt[i]->pass!=2)
            continue;
        delta = epicsTimeDiffInSeconds(&pcbt[i]->pass2Time, &pcbt[i]->pass1Time);
        error = delta - pcbt[i]->delay;
        if (fabs(error) >= 0.05) {
            slowups++;
            testDiag("delay %.02f seconds, delay error |%.04f| >= 0.05",
                pcbt[i]->delay, error);
        }
        updateStats(timeError[i%NUM_CALLBACK_PRIORITIES], error);
    }
    testOk(slowups < 5, "%d slowups during callbacks", slowups);

    testDiag("Setup time statistics");
    printStats(setupError[0], "LOW");
    printStats(setupError[1], "MID");
    printStats(setupError[2], "HIGH");

    testDiag("Delay time statistics");
    printStats(timeError[0], "LOW");
    printStats(timeError[1], "MID");
    printStats(timeError[2], "HIGH");

    for (i = 0; i < NCALLBACKS ; i++) {
        free(pcbt[i]);
    }

    callbackStop();
    callbackCleanup();

    return testDone();
}
