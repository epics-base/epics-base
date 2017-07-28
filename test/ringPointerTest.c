/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* ringPointerTest.c */

/* Author:  Marty Kraimer Date:    13OCT2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsRingPointer.h"
#include "errlog.h"
#include "epicsEvent.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static
void *int2ptr(size_t i)
{
    char *zero = 0;
    i = i|(i<<16);
    return zero+i;
}

static int foundCorruption;

static
size_t ptr2int(void *p)
{
    char *zero = 0, *p2 = p;
    size_t i = p2-zero;
    if((i&0xffff)!=((i>>16)&0xffff)) {
        testDiag("Pointer value corruption %p", p);
        foundCorruption = 1;
    }
    return i&0xffff;
}

static void testSingle(void)
{
    int i;
    const int rsize = 100;
    void *addr = 0;
    epicsRingPointerId ring = epicsRingPointerCreate(rsize);

    foundCorruption = 0;

    testDiag("Testing operations w/o threading");

    testOk1(epicsRingPointerIsEmpty(ring));
    testOk1(!epicsRingPointerIsFull(ring));
    testOk1(epicsRingPointerGetFree(ring)==rsize);
    testOk1(epicsRingPointerGetSize(ring)==rsize);
    testOk1(epicsRingPointerGetUsed(ring)==0);

    testOk1(epicsRingPointerPop(ring)==NULL);

    addr = int2ptr(1);
    testOk1(epicsRingPointerPush(ring, addr)==1);

    testOk1(!epicsRingPointerIsEmpty(ring));
    testOk1(!epicsRingPointerIsFull(ring));
    testOk1(epicsRingPointerGetFree(ring)==rsize-1);
    testOk1(epicsRingPointerGetSize(ring)==rsize);
    testOk1(epicsRingPointerGetUsed(ring)==1);

    testDiag("Fill it up");
    for(i=2; i<2*rsize; i++) {
        int ret;
        addr = int2ptr(i);
        ret = epicsRingPointerPush(ring, addr);
        if(!ret)
            break;
    }

    /* Note: +1 because we started with 1 */
    testOk(i==rsize+1, "%d == %d", i, rsize+1);
    testOk1(!epicsRingPointerIsEmpty(ring));
    testOk1(epicsRingPointerIsFull(ring));
    testOk1(epicsRingPointerGetFree(ring)==0);
    testOk1(epicsRingPointerGetSize(ring)==rsize);
    testOk1(epicsRingPointerGetUsed(ring)==rsize);

    testDiag("Drain it out");
    for(i=1; i<2*rsize; i++) {
        addr = epicsRingPointerPop(ring);
        if(addr==NULL || ptr2int(addr)!=i)
            break;
    }

    testOk1(!foundCorruption);

    testOk(i==rsize+1, "%d == %d", i, rsize+1);
    testOk1(epicsRingPointerIsEmpty(ring));
    testOk1(!epicsRingPointerIsFull(ring));
    testOk1(epicsRingPointerGetFree(ring)==rsize);
    testOk1(epicsRingPointerGetSize(ring)==rsize);
    testOk1(epicsRingPointerGetUsed(ring)==0);

    testDiag("Fill it up again");
    for(i=2; i<2*rsize; i++) {
        int ret;
        addr = int2ptr(i);
        ret = epicsRingPointerPush(ring, addr);
        if(!ret)
            break;
    }

    testDiag("flush");
    testOk1(epicsRingPointerIsFull(ring));
    epicsRingPointerFlush(ring);

    testOk1(epicsRingPointerIsEmpty(ring));
    testOk1(!epicsRingPointerIsFull(ring));
    testOk1(epicsRingPointerGetFree(ring)==rsize);
    testOk1(epicsRingPointerGetSize(ring)==rsize);
    testOk1(epicsRingPointerGetUsed(ring)==0);

    epicsRingPointerDelete(ring);
}

typedef struct {
    epicsRingPointerId ring;
    epicsEventId sync, wait;
    int stop;
    void *lastaddr;
} pairPvt;

static void pairConsumer(void *raw)
{
    pairPvt *pvt = raw;
    void *prev = pvt->lastaddr;

    epicsEventMustTrigger(pvt->sync);

    while(1) {
        size_t c,p;

        epicsEventMustWait(pvt->wait);
        if(pvt->stop)
            break;

        pvt->lastaddr = epicsRingPointerPop(pvt->ring);
        c = ptr2int(pvt->lastaddr);
        p = ptr2int(prev);
        if(p+1!=c) {
            testFail("consumer skip %p %p", prev, pvt->lastaddr);
            break;
        }
        prev = pvt->lastaddr;
    }

    pvt->stop = 1;
    epicsEventMustTrigger(pvt->sync);
}

static void testPair(int locked)
{
    unsigned int myprio = epicsThreadGetPrioritySelf(), consumerprio;
    pairPvt pvt;
    const int rsize = 100;
    int i, expect;
    epicsRingPointerId ring;
    if(locked)
        ring = epicsRingPointerLockedCreate(rsize);
    else
        ring = epicsRingPointerCreate(rsize);

    pvt.ring = ring;
    pvt.sync = epicsEventCreate(epicsEventEmpty);
    pvt.wait = epicsEventCreate(epicsEventEmpty);
    pvt.stop = 0;
    pvt.lastaddr = 0;

    foundCorruption = 0;

    testDiag("single producer, single consumer with%s locking", locked?"":"out");

    /* give the consumer thread a slightly higher priority so that
     * it can preempt us on RTOS targets.  On non-RTOS targets
     * we expect to be preempted at some random time
     */
    if(!epicsThreadLowestPriorityLevelAbove(myprio, &consumerprio))
        testAbort("Can't run test from thread with highest priority");

    epicsThreadMustCreate("pair", consumerprio,
                          epicsThreadGetStackSize(epicsThreadStackSmall),
                          &pairConsumer, &pvt);
    /* wait for worker to start */
    epicsEventMustWait(pvt.sync);

    i=1;
    while(i<rsize*10 && !pvt.stop) {
        int full = epicsRingPointerPush(ring, int2ptr(i));
        if(full || i%32==31)
            epicsEventMustTrigger(pvt.wait);
        if(full) {
            i++;
        }
    }

    testDiag("Everything enqueued, Stopping consumer");
    pvt.stop = 1;
    epicsEventMustTrigger(pvt.wait);
    epicsEventMustWait(pvt.sync);

    i = epicsRingPointerGetUsed(ring);
    testDiag("Pushed %d, have %d remaining unconsumed", rsize*10, i);
    expect = rsize*10-i;
    testDiag("Expect %d consumed", expect);

    testOk(pvt.lastaddr==int2ptr(expect-1), "%p == %p", pvt.lastaddr, int2ptr(expect-1));
    if(pvt.lastaddr==int2ptr(expect-1))
        testPass("Consumer consumed all");

    testOk1(!foundCorruption);

    epicsEventDestroy(pvt.sync);
    epicsEventDestroy(pvt.wait);
    epicsRingPointerDelete(ring);
}

MAIN(ringPointerTest)
{
    int prio = epicsThreadGetPrioritySelf();

    testPlan(37);
    testSingle();
    epicsThreadSetPriority(epicsThreadGetIdSelf(), epicsThreadPriorityScanLow);
    testPair(0);
    testPair(1);
    epicsThreadSetPriority(epicsThreadGetIdSelf(), prio);
    return testDone();
}
