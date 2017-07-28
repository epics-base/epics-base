/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>

#include "epicsTime.h"
#include "epicsAssert.h"
#include "bucketLib.h"
#include "testMain.h"

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

MAIN(buckTest)
{
    unsigned id1;
    unsigned id2;
    char * pValSave1;
    char * pValSave2;
    int s;
    BUCKET * pb;
    char * pVal;
    unsigned i;
    epicsTimeStamp start, finish;
    double duration;
    const int LOOPS = 500000;

    pb = bucketCreate(8);
    if (!pb) {
        return -1;
    }

    id1 = 0x1000a432;
    pValSave1 = "fred";
    s = bucketAddItemUnsignedId(pb, &id1, pValSave1);
    verify (s == S_bucket_success);

    pValSave2 = "jane";
    id2 = 0x0000a432;
    s = bucketAddItemUnsignedId(pb, &id2, pValSave2);
    verify (s == S_bucket_success);

    epicsTimeGetCurrent(&start);
    for (i=0; i<LOOPS; i++) {
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id1);
        verify (pVal == pValSave1);
        pVal = bucketLookupItemUnsignedId(pb, &id2);
        verify (pVal == pValSave2);
    }
    epicsTimeGetCurrent(&finish);

    duration = epicsTimeDiffInSeconds(&finish, &start);
    printf("%d loops took %.10f seconds\n", LOOPS, duration);
    duration /= 10 * LOOPS;
    printf("which is %.3f nanoseconds per hash lookup\n", duration * 1e9);

    bucketShow(pb);

    return S_bucket_success;
}
