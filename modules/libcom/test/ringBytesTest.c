/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* ringBytesTest.c */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsRingBytes.h"
#include "errlog.h"
#include "epicsEvent.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define RINGSIZE 10

typedef struct info {
    epicsEventId consumerEvent;
    epicsRingBytesId	ring;
}info;

static void check(epicsRingBytesId ring, int expectedFree)
{
    int expectedUsed = RINGSIZE - expectedFree;
    int expectedEmpty = (expectedUsed == 0);
    int expectedFull = (expectedFree == 0);
    int nFree = epicsRingBytesFreeBytes(ring);
    int nUsed = epicsRingBytesUsedBytes(ring);
    int isEmpty = epicsRingBytesIsEmpty(ring);
    int isFull = epicsRingBytesIsFull(ring);
    
    testOk(nFree == expectedFree, "Free: %d == %d", nFree, expectedFree);
    testOk(nUsed == expectedUsed, "Used: %d == %d", nUsed, expectedUsed);
    testOk(isEmpty == expectedEmpty, "Empty: %d == %d", isEmpty, expectedEmpty);
    testOk(isFull == expectedFull, "Full: %d == %d", isFull, expectedFull);
}
    
MAIN(ringBytesTest)
{
    int i, n;
    info *pinfo;
    epicsEventId consumerEvent;
    char put[RINGSIZE+1];
    char get[RINGSIZE+1];
    epicsRingBytesId ring;

    testPlan(245);

    pinfo = calloc(1,sizeof(info));
    if (!pinfo) {
        testAbort("calloc failed");
    }
    pinfo->consumerEvent = consumerEvent = epicsEventCreate(epicsEventEmpty);
    if (!consumerEvent) {
        testAbort("epicsEventCreate failed");
    }

    pinfo->ring = ring = epicsRingBytesCreate(RINGSIZE);
    if (!ring) {
        testAbort("epicsRingBytesCreate failed");
    }
    check(ring, RINGSIZE);

    for (i = 0 ; i < sizeof(put) ; i++)
        put[i] = i;
    for(i = 0 ; i < RINGSIZE ; i++) {
        n = epicsRingBytesPut(ring, put, i);
        testOk(n==i, "ring put %d", i);
        check(ring, RINGSIZE-i);
        n = epicsRingBytesGet(ring, get, i);
        testOk(n==i, "ring get %d", i);
        check(ring, RINGSIZE);
        testOk(memcmp(put,get,i)==0, "get matches write");
    }

    for(i = 0 ; i < RINGSIZE ; i++) {
        n = epicsRingBytesPut(ring, put+i, 1);
        testOk(n==1, "ring put 1, %d", i);
        check(ring, RINGSIZE-1-i);
    }
    n = epicsRingBytesPut(ring, put+RINGSIZE, 1);
    testOk(n==0, "put to full ring");
    check(ring, 0);
    for(i = 0 ; i < RINGSIZE ; i++) {
        n = epicsRingBytesGet(ring, get+i, 1);
        testOk(n==1, "ring get 1, %d", i);
        check(ring, 1+i);
    }
    testOk(memcmp(put,get,RINGSIZE)==0, "get matches write");
    n = epicsRingBytesGet(ring, get+RINGSIZE, 1);
    testOk(n==0, "get from empty ring");
    check(ring, RINGSIZE);

    n = epicsRingBytesPut(ring, put, RINGSIZE+1);
    testOk(n==0, "ring put beyond ring capacity (%d, expected 0)",n);
    check(ring, RINGSIZE);
    n = epicsRingBytesPut(ring, put, 1);
    testOk(n==1, "ring put %d", 1);
    check(ring, RINGSIZE-1);
    n = epicsRingBytesPut(ring, put, RINGSIZE);
    testOk(n==0, "ring put beyond ring capacity (%d, expected 0)",n);
    check(ring, RINGSIZE-1);
    n = epicsRingBytesGet(ring, get, 1);
    testOk(n==1, "ring get %d", 1);
    check(ring, RINGSIZE);

    epicsRingBytesDelete(ring);
    epicsEventDestroy(consumerEvent);
    free(pinfo);

    return testDone();
}
