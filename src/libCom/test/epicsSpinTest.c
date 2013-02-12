/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Ralph Lange <Ralph.Lange@gmx.de>
 *
 * based on epicsMutexTest by Marty Kraimer and Jeff Hill
 *
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsSpin.h"
#include "epicsEvent.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct info {
    int         threadnum;
    epicsSpinId spin;
    int         quit;
} info;

void spinThread(void *arg)
{
    info *pinfo = (info *) arg;
    testDiag("spinThread %d starting", pinfo->threadnum);
    while (pinfo->quit--) {
        epicsSpinLock(pinfo->spin);
        testPass("spinThread %d epicsSpinLock taken", pinfo->threadnum);
        epicsThreadSleep(.1);
        epicsSpinUnlock(pinfo->spin);
        epicsThreadSleep(.9);
    }
    testDiag("spinThread %d exiting", pinfo->threadnum);
    return;
}

static void lockPair(struct epicsSpin *spin)
{
    epicsSpinLock(spin);
    epicsSpinUnlock(spin);
}

static void tenLockPairs(struct epicsSpin *spin)
{
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
    lockPair(spin);
}

static void tenLockPairsSquared(struct epicsSpin *spin)
{
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
    tenLockPairs(spin);
}

void epicsSpinPerformance ()
{
    static const unsigned N = 10000;
    unsigned i;
    epicsSpinId spin;
    epicsTimeStamp begin;
    epicsTimeStamp end;
    double delay;

    /* Initialize spinlock */
    spin = epicsSpinCreate();

    /* test a single lock pair */
    epicsTimeGetCurrent(&begin);
    for ( i = 0; i < N; i++ ) {
        tenLockPairsSquared(spin);
    }
    epicsTimeGetCurrent(&end);

    delay = epicsTimeDiffInSeconds(&end, &begin);
    delay /= N * 100u;  /* convert to delay per lock pair */
    delay *= 1e6;       /* convert to micro seconds */
    testDiag("lock()*1/unlock()*1 takes %f microseconds", delay);
}

struct verifyTryLock {
    epicsSpinId  spin;
    epicsEventId done;
};

static void verifyTryLockThread(void *pArg)
{
    struct verifyTryLock *pVerify =
        (struct verifyTryLock *) pArg;

    testOk1(epicsSpinTryLock(pVerify->spin));
    epicsEventSignal(pVerify->done);
}

static void verifyTryLock()
{
    struct verifyTryLock verify;

    verify.spin = epicsSpinCreate();
    verify.done = epicsEventMustCreate(epicsEventEmpty);

    testOk1(epicsSpinTryLock(verify.spin) == 0);

    epicsThreadCreate("verifyTryLockThread", 40,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        verifyTryLockThread, &verify);

    testOk1(epicsEventWait(verify.done) == epicsEventWaitOK);

    epicsSpinUnlock(verify.spin);
    epicsSpinDestroy(verify.spin);
    epicsEventDestroy(verify.done);
}

MAIN(epicsSpinTest)
{
    const int nthreads = 3;
    const int nrounds = 5;
    unsigned int stackSize;
    epicsThreadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    epicsSpinId spin;

    testPlan(3 + nthreads * nrounds);

    verifyTryLock();

    spin = epicsSpinCreate();

    id = (epicsThreadId *) calloc(nthreads, sizeof(epicsThreadId));
    name = (char **) calloc(nthreads, sizeof(char *));
    arg = (void **) calloc(nthreads, sizeof(void *));
    pinfo = (info **) calloc(nthreads, sizeof(info *));
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    for (i = 0; i < nthreads; i++) {
        name[i] = (char *) calloc(10, sizeof(char));
        sprintf(name[i],"task%d",i);
        pinfo[i] = (info *) calloc(1, sizeof(info));
        pinfo[i]->threadnum = i;
        pinfo[i]->spin = spin;
        pinfo[i]->quit = nrounds;
        arg[i] = pinfo[i];
        id[i] = epicsThreadCreate(name[i], 40, stackSize,
                                  spinThread,
                                  arg[i]);
    }
    epicsThreadSleep(2.0 + nrounds);

    epicsSpinPerformance();

    return testDone();
}
