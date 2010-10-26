/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsMutexTest.c */

/* 
 * Author:  Marty Kraimer Date:    26JAN2000 
 *          Jeff Hill (added mutex performance test )
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct info {
    int        threadnum;
    epicsMutexId mutex;
    int        quit;
}info;

extern "C" void mutexThread(void * arg)
{
    info *pinfo = (info *)arg;
    testDiag("mutexThread %d starting", pinfo->threadnum);
    while (pinfo->quit--) {
        epicsMutexLockStatus status = epicsMutexLock(pinfo->mutex);
        testOk(status == epicsMutexLockOK,
            "mutexThread %d epicsMutexLock returned %d",
            pinfo->threadnum, (int)status);
        epicsThreadSleep(.1);
        epicsMutexUnlock(pinfo->mutex);
        epicsThreadSleep(.9);
    }
    testDiag("mutexThread %d exiting", pinfo->threadnum);
    return;
}

inline void lockPair ( epicsMutex & mutex )
{
    mutex.lock ();
    mutex.unlock ();
}

inline void tenLockPairs ( epicsMutex & mutex )
{
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
}

inline void tenLockPairsSquared ( epicsMutex & mutex )
{
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
}

inline void doubleRecursiveLockPair ( epicsMutex & mutex )
{
    mutex.lock ();
    mutex.lock ();
    mutex.unlock ();
    mutex.unlock ();
}

inline void tenDoubleRecursiveLockPairs ( epicsMutex & mutex )
{
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
    doubleRecursiveLockPair ( mutex );
}

inline void tenDoubleRecursiveLockPairsSquared ( epicsMutex & mutex )
{
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
    tenDoubleRecursiveLockPairs ( mutex );
}

inline void quadRecursiveLockPair ( epicsMutex & mutex )
{
    mutex.lock ();
    mutex.lock ();
    mutex.lock ();
    mutex.lock ();
    mutex.unlock ();
    mutex.unlock ();
    mutex.unlock ();
    mutex.unlock ();
}

inline void tenQuadRecursiveLockPairs ( epicsMutex & mutex )
{
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
    quadRecursiveLockPair ( mutex );
}

inline void tenQuadRecursiveLockPairsSquared ( epicsMutex & mutex )
{
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
    tenQuadRecursiveLockPairs ( mutex );
}

void epicsMutexPerformance ()
{
    epicsMutex mutex;
	unsigned i;

    // test a single lock pair
    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 1000;
    for ( i = 0; i < N; i++ ) {
        tenLockPairsSquared ( mutex );
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= N * 100u; // convert to delay per lock pair
    delay *= 1e6; // convert to micro seconds
    testDiag("lock()*1/unlock()*1 takes %f microseconds", delay);

    // test a two times recursive lock pair
    begin = epicsTime::getCurrent ();
    for ( i = 0; i < N; i++ ) {
        tenDoubleRecursiveLockPairsSquared ( mutex );
    }
    delay = epicsTime::getCurrent () -  begin;
    delay /= N * 100u; // convert to delay per lock pair
    delay *= 1e6; // convert to micro seconds
    testDiag("lock()*2/unlock()*2 takes %f microseconds", delay);

    // test a four times recursive lock pair
    begin = epicsTime::getCurrent ();
    for ( i = 0; i < N; i++ ) {
        tenQuadRecursiveLockPairsSquared ( mutex );
    }
    delay = epicsTime::getCurrent () -  begin;
    delay /= N * 100u; // convert to delay per lock pair
    delay *= 1e6; // convert to micro seconds
    testDiag("lock()*4/unlock()*4 takes %f microseconds", delay);
}

struct verifyTryLock {
    epicsMutexId mutex;
    epicsEventId done;
};

extern "C" void verifyTryLockThread ( void *pArg )
{
    struct verifyTryLock *pVerify = 
        ( struct verifyTryLock * ) pArg;

    testOk1(epicsMutexTryLock(pVerify->mutex) == epicsMutexLockTimeout);
    epicsEventSignal ( pVerify->done );
}

void verifyTryLock ()
{
    struct verifyTryLock verify;

    verify.mutex = epicsMutexMustCreate ();
    verify.done = epicsEventMustCreate ( epicsEventEmpty );

    testOk1(epicsMutexTryLock(verify.mutex) == epicsMutexLockOK);

    epicsThreadCreate ( "verifyTryLockThread", 40, 
        epicsThreadGetStackSize(epicsThreadStackSmall),
        verifyTryLockThread, &verify );

    testOk1(epicsEventWait ( verify.done ) == epicsEventWaitOK);

    epicsMutexUnlock ( verify.mutex );
    epicsMutexDestroy ( verify.mutex );
    epicsEventDestroy ( verify.done );
}

MAIN(epicsMutexTest)
{
    const int nthreads = 3;
    const int nrounds = 5;
    unsigned int stackSize;
    epicsThreadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    epicsMutexId mutex;
    int status;

    testPlan(5 + nthreads * nrounds);

    verifyTryLock ();

    mutex = epicsMutexMustCreate();
    status = epicsMutexLock(mutex);
    testOk(status == 0, "epicsMutexLock returned %d", status);
    status = epicsMutexTryLock(mutex);
    testOk(status == 0, "epicsMutexTryLock returned %d", status);
    epicsMutexUnlock(mutex);
    epicsMutexUnlock(mutex);

    id = (epicsThreadId *)calloc(nthreads,sizeof(epicsThreadId));
    name = (char **)calloc(nthreads,sizeof(char *));
    arg = (void **)calloc(nthreads,sizeof(void *));
    pinfo = (info **)calloc(nthreads,sizeof(info *));
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    for(i=0; i<nthreads; i++) {
        name[i] = (char *)calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        pinfo[i] = (info *)calloc(1,sizeof(info));
        pinfo[i]->threadnum = i;
        pinfo[i]->mutex = mutex;
        pinfo[i]->quit = nrounds;
        arg[i] = pinfo[i];
        id[i] = epicsThreadCreate(name[i],40,stackSize,
                                  mutexThread,
                                  arg[i]);
    }
    epicsThreadSleep(2.0 + nrounds);

    epicsMutexPerformance ();

    return testDone();
}
