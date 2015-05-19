/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include <string.h>

#include "dbScan.h"
#include "epicsEvent.h"

#include "dbUnitTest.h"
#include "testMain.h"

#include "dbAccess.h"
#include "errlog.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static epicsEventId waiter;
static int called;
static dbCommon *prec;

static void onceComp(void *junk, dbCommon *prec)
{
    testOk1(junk==(void*)&waiter);
    testOk1(strcmp(prec->name, "reca")==0);
    called = 1;
    epicsEventMustTrigger(waiter);
}

static void testOnce(void)
{
    testDiag("check scanOnceCallback() callback");
    waiter = epicsEventMustCreate(epicsEventEmpty);

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    prec = testdbRecordPtr("reca");

    testDiag("scanOnce %s", prec->name);
    scanOnceCallback(prec, onceComp, &waiter);
    testDiag("Waiting");
    epicsEventMustWait(waiter);
    testOk1(called==1);
    if(!called)
        testSkip(2, "callback failed to run");

    testIocShutdownOk();

    testdbCleanup();
    epicsEventDestroy(waiter);
}

MAIN(dbScanTest)
{
    testPlan(3);
    testOnce();
    return testDone();
}
