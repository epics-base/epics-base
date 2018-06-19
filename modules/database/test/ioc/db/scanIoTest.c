/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2013 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "epicsEvent.h"
#include "epicsMessageQueue.h"
#include "epicsPrint.h"
#include "epicsMath.h"
#include "alarm.h"
#include "menuPriority.h"
#include "dbChannel.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "dbScan.h"
#include "dbLock.h"
#include "dbUnitTest.h"
#include "dbCommon.h"
#include "recSup.h"
#include "devSup.h"
#include "iocInit.h"
#include "callback.h"
#include "ellLib.h"
#include "epicsUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

#include "epicsExport.h"

#include "devx.h"
#include "xRecord.h"

STATIC_ASSERT(NUM_CALLBACK_PRIORITIES==3);

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void loadRecord(int group, int member, const char *prio)
{
    char buf[40];
    sprintf(buf, "GROUP=%d,MEMBER=%d,PRIO=%s",
            group, member, prio);
    testdbReadDatabase("scanIoTest.db", NULL, buf);
}

typedef struct {
    int hasprocd[NUM_CALLBACK_PRIORITIES];
    int getcomplete[NUM_CALLBACK_PRIORITIES];
    epicsEventId wait[NUM_CALLBACK_PRIORITIES];
    epicsEventId wake[NUM_CALLBACK_PRIORITIES];
} testsingle;

static void testcb(xpriv *priv, void *raw)
{
    testsingle *td = raw;
    int prio = priv->prec->prio;

    testOk1(td->hasprocd[prio]==0);
    td->hasprocd[prio] = 1;
}

static void testcomp(void *raw, IOSCANPVT scan, int prio)
{
    testsingle *td = raw;

    testOk1(td->hasprocd[prio]==1);
    testOk1(td->getcomplete[prio]==0);
    td->getcomplete[prio] = 1;
    epicsEventMustTrigger(td->wait[prio]);
    epicsEventMustWait(td->wake[prio]);
}

static void testSingleThreading(void)
{
    int i;
    testsingle data[2];
    xdrv *drvs[2];

    memset(data, 0, sizeof(data));

    for(i=0; i<2; i++) {
        int p;
        for(p=0; p<NUM_CALLBACK_PRIORITIES; p++) {
            data[i].wake[p] = epicsEventMustCreate(epicsEventEmpty);
            data[i].wait[p] = epicsEventMustCreate(epicsEventEmpty);
        }
    }

    testDiag("Test single-threaded I/O Intr scanning");

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    /* create two scan lists with one record on each of three priorities */

    /* group#, member#, priority */
    loadRecord(0, 0, "LOW");
    loadRecord(1, 0, "LOW");
    loadRecord(0, 1, "MEDIUM");
    loadRecord(1, 1, "MEDIUM");
    loadRecord(0, 2, "HIGH");
    loadRecord(1, 2, "HIGH");

    drvs[0] = xdrv_add(0, &testcb, &data[0]);
    drvs[1] = xdrv_add(1, &testcb, &data[1]);
    scanIoSetComplete(drvs[0]->scan, &testcomp, &data[0]);
    scanIoSetComplete(drvs[1]->scan, &testcomp, &data[1]);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Scan first list");
    scanIoRequest(drvs[0]->scan);
    testDiag("Scan second list");
    scanIoRequest(drvs[1]->scan);

    testDiag("Wait for first list to complete");
    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++)
        epicsEventMustWait(data[0].wait[i]);

    testDiag("Wait one more second");
    epicsThreadSleep(1.0);

    testOk1(data[0].hasprocd[0]==1);
    testOk1(data[0].hasprocd[1]==1);
    testOk1(data[0].hasprocd[2]==1);
    testOk1(data[0].getcomplete[0]==1);
    testOk1(data[0].getcomplete[1]==1);
    testOk1(data[0].getcomplete[2]==1);
    testOk1(data[1].hasprocd[0]==0);
    testOk1(data[1].hasprocd[1]==0);
    testOk1(data[1].hasprocd[2]==0);
    testOk1(data[1].getcomplete[0]==0);
    testOk1(data[1].getcomplete[1]==0);
    testOk1(data[1].getcomplete[2]==0);

    testDiag("Release the first scan and wait for the second");
    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++)
        epicsEventMustTrigger(data[0].wake[i]);
    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++)
        epicsEventMustWait(data[1].wait[i]);

    testOk1(data[0].hasprocd[0]==1);
    testOk1(data[0].hasprocd[1]==1);
    testOk1(data[0].hasprocd[2]==1);
    testOk1(data[0].getcomplete[0]==1);
    testOk1(data[0].getcomplete[1]==1);
    testOk1(data[0].getcomplete[2]==1);
    testOk1(data[1].hasprocd[0]==1);
    testOk1(data[1].hasprocd[1]==1);
    testOk1(data[1].hasprocd[2]==1);
    testOk1(data[1].getcomplete[0]==1);
    testOk1(data[1].getcomplete[1]==1);
    testOk1(data[1].getcomplete[2]==1);

    testDiag("Release the second scan and complete");
    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++)
        epicsEventMustTrigger(data[1].wake[i]);

    testIocShutdownOk();

    testdbCleanup();

    xdrv_reset();

    for(i=0; i<2; i++) {
        int p;
        for(p=0; p<NUM_CALLBACK_PRIORITIES; p++) {
            epicsEventDestroy(data[i].wake[p]);
            epicsEventDestroy(data[i].wait[p]);
        }
    }
}

typedef struct {
    int hasprocd;
    int getcomplete;
    epicsEventId wait;
    epicsEventId wake;
} testmulti;

static void testcbmulti(xpriv *priv, void *raw)
{
    testmulti *td = raw;
    td += priv->member;

    testOk1(td->hasprocd==0);
    td->hasprocd = 1;
    epicsEventMustTrigger(td->wait);
    epicsEventMustWait(td->wake);
    td->getcomplete = 1;
}

static void testcompmulti(void *raw, IOSCANPVT scan, int prio)
{
    int *mask = raw;
    testOk(((*mask)&(1<<prio))==0, "(0x%x)&(0x%x)==0", *mask, 1<<prio);
    *mask |= 1<<prio;
}

static void testMultiThreading(void)
{
    int i;
    int masks[2];
    testmulti data[6];
    xdrv *drvs[2];

    memset(masks, 0, sizeof(masks));
    memset(data, 0, sizeof(data));

    for(i=0; i<NELEMENTS(data); i++) {
        data[i].wake = epicsEventMustCreate(epicsEventEmpty);
        data[i].wait = epicsEventMustCreate(epicsEventEmpty);
    }

    testDiag("Test multi-threaded I/O Intr scanning");

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    /* create two scan lists with one record on each of three priorities */

    /* group#, member#, priority */
    loadRecord(0, 0, "LOW");
    loadRecord(1, 1, "LOW");
    loadRecord(0, 2, "MEDIUM");
    loadRecord(1, 3, "MEDIUM");
    loadRecord(0, 4, "HIGH");
    loadRecord(1, 5, "HIGH");

    drvs[0] = xdrv_add(0, &testcbmulti, data);
    drvs[1] = xdrv_add(1, &testcbmulti, data);
    scanIoSetComplete(drvs[0]->scan, &testcompmulti, &masks[0]);
    scanIoSetComplete(drvs[1]->scan, &testcompmulti, &masks[1]);

    /* just enough workers to process all records concurrently */
    callbackParallelThreads(2, "LOW");
    callbackParallelThreads(2, "MEDIUM");
    callbackParallelThreads(2, "HIGH");

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Scan first list");
    testOk1(scanIoRequest(drvs[0]->scan)==0x7);
    testDiag("Scan second list");
    testOk1(scanIoRequest(drvs[1]->scan)==0x7);

    testDiag("Wait for everything to start");
    for(i=0; i<NELEMENTS(data); i++)
        epicsEventMustWait(data[i].wait);

    testDiag("Wait one more second");
    epicsThreadSleep(1.0);

    for(i=0; i<NELEMENTS(data); i++) {
        testOk(data[i].hasprocd==1, "data[%d].hasprocd==1 (%d)", i, data[i].hasprocd);
        testOk(data[i].getcomplete==0, "data[%d].getcomplete==0 (%d)", i, data[i].getcomplete);
    }

    testDiag("Release all and complete");
    for(i=0; i<NELEMENTS(data); i++)
        epicsEventMustTrigger(data[i].wake);

    testIocShutdownOk();

    for(i=0; i<NELEMENTS(data); i++) {
        testOk(data[i].getcomplete==1, "data[%d].getcomplete==0 (%d)", i, data[i].getcomplete);
    }
    testOk1(masks[0]==0x7);
    testOk1(masks[1]==0x7);

    testdbCleanup();

    xdrv_reset();

    for(i=0; i<NELEMENTS(data); i++) {
        epicsEventDestroy(data[i].wake);
        epicsEventDestroy(data[i].wait);
    }
}

MAIN(scanIoTest)
{
    testPlan(152);
    testSingleThreading();
    testDiag("run a second time to verify shutdown and restart works");
    testSingleThreading();
    testMultiThreading();
    testDiag("run a second time to verify shutdown and restart works");
    testMultiThreading();
    return testDone();
}
