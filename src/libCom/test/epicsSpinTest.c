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
#include "epicsAtomic.h"
#include "epicsSpin.h"
#include "epicsEvent.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct info {
    int         threadnum;
    epicsSpinId spin;
    int         *counter;
    int         rounds;
    epicsEventId done;
} info;

#define spinDelay 0.016667

void spinThread(void *arg)
{
    info *pinfo = (info *) arg;
    testDiag("spinThread %d starting", pinfo->threadnum);
    epicsThreadSleep(0.1);  /* Try to align threads */
    while (pinfo->rounds--) {
        epicsThreadSleep(spinDelay);
        epicsSpinLock(pinfo->spin);
        pinfo->counter[0]++;
        epicsSpinUnlock(pinfo->spin);
    }
    testDiag("spinThread %d exiting", pinfo->threadnum);
    epicsEventSignal(pinfo->done);
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
    if (!spin)
        testAbort("epicsSpinCreate() returned NULL");

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
    epicsSpinDestroy(spin);
}

struct verifyTryLock;

struct verifyTryLockEnt {
    epicsEventId done;
    struct verifyTryLock *main;
};

struct verifyTryLock {
    epicsSpinId  spin;
    int flag;
    struct verifyTryLockEnt *ents;
};

static void verifyTryLockThread(void *pArg)
{
    struct verifyTryLockEnt *pVerify =
        (struct verifyTryLockEnt *) pArg;

    while(epicsAtomicGetIntT(&pVerify->main->flag)==0) {
        int ret = epicsSpinTryLock(pVerify->main->spin);
        if(ret!=0) {
            epicsAtomicCmpAndSwapIntT(&pVerify->main->flag, 0, ret);
            break;
        } else
            epicsSpinUnlock(pVerify->main->spin);
    }

    epicsEventSignal(pVerify->done);
}

/* Start one thread per CPU which will all try lock
 * the same spinlock.  They break as soon as one
 * fails to take the lock.
 */
static void verifyTryLock()
{
    int N, i;
    struct verifyTryLock verify;

    N = epicsThreadGetCPUs();
    if(N==1) {
        testSkip(1, "verifyTryLock() only for SMP systems");
        return;
    }

    verify.flag = 0;
    verify.spin = epicsSpinMustCreate();

    testDiag("Starting %d spinners", N);

    verify.ents = calloc(N, sizeof(*verify.ents));
    for(i=0; i<N; i++) {
        verify.ents[i].main = &verify;
        verify.ents[i].done = epicsEventMustCreate(epicsEventEmpty);
        epicsThreadMustCreate("verifyTryLockThread", 40,
            epicsThreadGetStackSize(epicsThreadStackSmall),
                              verifyTryLockThread, &verify.ents[i]);
    }

    testDiag("All started");

    for(i=0; i<N; i++) {
        epicsEventMustWait(verify.ents[i].done);
        epicsEventDestroy(verify.ents[i].done);
    }

    testDiag("All done");

    testOk(verify.flag==1, "epicsTryLock returns %d (expect 1)", verify.flag);

    epicsSpinDestroy(verify.spin);
    free(verify.ents);
}

MAIN(epicsSpinTest)
{
    const int nthreads = 3;
    const int nrounds = 500;
    unsigned int stackSize;
    epicsThreadId *id;
    int i, counter;
    char **name;
    info **pinfo;
    epicsSpinId spin;

    testPlan(2);

    verifyTryLock();

    spin = epicsSpinCreate();
    if (!spin)
        testAbort("epicsSpinCreate() returned NULL");

    id = (epicsThreadId *) calloc(nthreads, sizeof(epicsThreadId));
    name = (char **) calloc(nthreads, sizeof(char *));
    pinfo = (info **) calloc(nthreads, sizeof(info *));
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    counter = 0;
    for (i = 0; i < nthreads; i++) {
        name[i] = (char *) calloc(10, sizeof(char));
        sprintf(name[i],"task%d",i);
        pinfo[i] = (info *) calloc(1, sizeof(info));
        pinfo[i]->threadnum = i;
        pinfo[i]->spin = spin;
        pinfo[i]->counter = &counter;
        pinfo[i]->rounds = nrounds;
        pinfo[i]->done = epicsEventMustCreate(epicsEventEmpty);
        id[i] = epicsThreadCreate(name[i], 40, stackSize,
                                  spinThread,
                                  pinfo[i]);
    }
    for (i = 0; i < nthreads; i++) {
        epicsEventMustWait(pinfo[i]->done);
        epicsEventDestroy(pinfo[i]->done);
        free(name[i]);
        free(pinfo[i]);
    }
    testOk(counter == nthreads * nrounds, "Loops run = %d (expecting %d)",
        counter, nthreads * nrounds);

    free(pinfo);
    free(name);
    free(id);
    epicsSpinDestroy(spin);

    epicsSpinPerformance();

    return testDone();
}
