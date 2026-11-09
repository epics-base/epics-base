/*************************************************************************\
* Copyright (c) 2020 Gabriel Fedel
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "registryFunction.h"
#include "subRecord.h"
#include <epicsEvent.h>
#include <epicsThread.h>

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static int trace;
static epicsEventId seqAComplete;
static epicsEventId seqBComplete;

static long seqTrace(subRecord *prec)
{
    trace = trace * 10 + (int)prec->val;
    return 0;
}

static long seqACompleteSub(subRecord *prec)
{
    epicsEventMustTrigger(seqAComplete);
    return 0;
}

static long seqBCompleteSub(subRecord *prec)
{
    epicsEventMustTrigger(seqBComplete);
    return 0;
}

/*
 * The DLYn fields advertise a display range of -1 to 10 seconds
 * */
static
void testSeqDlyLimits(void){
    DBADDR addr;
    struct {
        DBRgrDouble
        epicsFloat64 value;
    } grbuf;
    struct {
        DBRctrlDouble
        epicsFloat64 value;
    } ctrlbuf;
    long options = DBR_GR_DOUBLE;
    long nRequest = 1;

    testOk1(!dbNameToAddr("seq0.DLY0", &addr));
    testOk1(!dbGetField(&addr, DBR_DOUBLE, &grbuf, &options, &nRequest, NULL));
    testOk(grbuf.lower_disp_limit == -1.0,
        "DLY0 lower_disp_limit (%g) == -1", grbuf.lower_disp_limit);
    testOk(grbuf.upper_disp_limit == 10.0,
        "DLY0 upper_disp_limit (%g) == 10", grbuf.upper_disp_limit);

    options = DBR_CTRL_DOUBLE;
    testOk1(!dbGetField(&addr, DBR_DOUBLE, &ctrlbuf, &options, &nRequest, NULL));
    testOk(ctrlbuf.lower_ctrl_limit == -1.0,
        "DLY0 lower_ctrl_limit (%g) == -1", ctrlbuf.lower_ctrl_limit);
}

/*
 * This test verifies the behavior of seq using Specified for SELM
 * The behavior should be the same for all the DOLx
 * */
static
void testSeqSpecified(void){
    int i;
    for (i=0; i < 16; i++) {
        testdbPutFieldOk("seq0.SELN", DBR_USHORT, i);

        testdbPutFieldOk("ai0", DBR_LONG, 0);

        testdbPutFieldOk("seq0.PROC", DBR_USHORT, 1);

        testSyncCallback();
        testdbGetFieldEqual("ai0", DBR_LONG, i+1);
    }
}

static
void testSeqNegativeDelay(void)
{
    trace = 0;
    testdbPutFieldOk("seqA.PROC", DBR_LONG, 1);
    epicsEventMustWait(seqAComplete);
    epicsEventMustWait(seqBComplete);
    testOk(trace == 1324, "zero delays allow link groups to interleave");

    testdbPutFieldOk("seqA.DLY1", DBR_DOUBLE, -1.0);
    trace = 0;
    testdbPutFieldOk("seqA.PROC", DBR_LONG, 1);
    epicsEventMustWait(seqAComplete);
    epicsEventMustWait(seqBComplete);
    testOk(trace == 1234,
        "negative delay keeps following group in the current lockset");

    testdbPutFieldOk("seqA.DLY1", DBR_DOUBLE, 0.01);
    trace = 0;
    testdbPutFieldOk("seqA.PROC", DBR_LONG, 1);
    epicsEventMustWait(seqAComplete);
    epicsEventMustWait(seqBComplete);
    testOk(trace == 1342, "positive delay retains delayed callback behavior");

    testdbPutFieldOk("seqSync.DLY0", DBR_DOUBLE, -1.0);
    testdbPutFieldOk("seqSync.DLY1", DBR_DOUBLE, -1.0);
    trace = 0;
    testdbPutFieldOk("seqSync.PROC", DBR_LONG, 1);
    testOk(trace == 56,
        "initial synchronous groups run before the initiating put returns");
    testdbGetFieldEqual("seqSync.PACT", DBR_USHORT, 0);
}


MAIN(eventTest) {
    testPlan(4*16 + 19);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("seqTrace", (REGISTRYFUNCTION) seqTrace);
    registryFunctionAdd("seqACompleteSub", (REGISTRYFUNCTION) seqACompleteSub);
    registryFunctionAdd("seqBCompleteSub", (REGISTRYFUNCTION) seqBCompleteSub);
    seqAComplete = epicsEventMustCreate(epicsEventEmpty);
    seqBComplete = epicsEventMustCreate(epicsEventEmpty);

    testdbReadDatabase("seqTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testSeqDlyLimits();
    testSeqSpecified();
    testSeqNegativeDelay();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
