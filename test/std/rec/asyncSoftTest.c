/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as operator of Argonne
* National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "dbStaticLib.h"
#include "dbTest.h"
#include "dbUnitTest.h"
#include "epicsEvent.h"
#include "errlog.h"
#include "registryFunction.h"
#include "subRecord.h"
#include "testMain.h"

static int startCounter, doneCounter;
static epicsEventId asyncEvent, doneEvent;

static
long asyncSubr(subRecord *prec)
{
    testDiag("Processing %s, pact=%d", prec->name, prec->pact);

    if (!prec->pact) {
        epicsEventTrigger(asyncEvent);
        prec->pact = 1;     /* Make asynchronous */
    }

    return 0;
}

static
long doneSubr(subRecord *prec)
{
    epicsEventTrigger(doneEvent);
    return 0;
}

static
void checkAsyncInput(const char *rec, int init, dbCommon *async)
{
    char inp[16], proc[16];

    testDiag("Checking record '%s'", rec);

    strcpy(inp, rec);
    strcat(inp, ".INP");
    strcpy(proc, rec);
    strcat(proc, ".PROC");

    if (init) {
        testdbGetFieldEqual(rec, DBF_LONG, init);

        testdbPutFieldOk(inp, DBF_STRING, "async");
    }

    testdbPutFieldOk(proc, DBF_CHAR, 1);

    epicsEventWait(asyncEvent);
    testdbGetFieldEqual("startCounter", DBF_LONG, ++startCounter);
    testdbGetFieldEqual("doneCounter", DBF_LONG, doneCounter);

    dbScanLock(async);
    async->rset->process(async);
    dbScanUnlock(async);

    epicsEventWait(doneEvent);
    testdbGetFieldEqual("startCounter", DBF_LONG, startCounter);
    testdbGetFieldEqual("doneCounter", DBF_LONG, ++doneCounter);
}

static
void testAsynInputs(dbCommon *async)
{
    const char * records[] = {
        "ai0", "bi0", "di0", "ii0", "li0", "mi0", "si0", NULL,
        "bi1",  /* bi1 must be first in this group */
        "ai1", "di1", "ii1", "li1", "mi1", "si1", NULL,
    };
    const char ** rec = &records[0];
    int init = 1;   /* bi1 initializes to 1 */

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    startCounter = doneCounter = 0;
    testdbPutFieldOk("startCounter", DBF_LONG, startCounter);
    testdbPutFieldOk("doneCounter", DBF_LONG, doneCounter);

    epicsEventTryWait(asyncEvent);
    epicsEventTryWait(doneEvent);

    while (*rec) {  /* 1st group don't need initializing */
        checkAsyncInput(*rec++, 0, async);
    }
    rec++;
    while (*rec) {
        checkAsyncInput(*rec++, init, async);
        init = 9;   /* remainder initialize to 9 */
    }

    testDiag("============= Ending %s =============", EPICS_FUNCTION);
}

static
void checkAsyncOutput(const char *rec, dbCommon *async)
{
    char proc[16];

    testDiag("Checking record '%s'", rec);

    strcpy(proc, rec);
    strcat(proc, ".PROC");

    testdbPutFieldOk(proc, DBF_CHAR, 1);

    epicsEventWait(asyncEvent);
    testdbGetFieldEqual("startCounter", DBF_LONG, ++startCounter);
    testdbGetFieldEqual("doneCounter", DBF_LONG, doneCounter);

    dbScanLock(async);
    async->rset->process(async);
    dbScanUnlock(async);

    epicsEventWait(doneEvent);
    testdbGetFieldEqual("startCounter", DBF_LONG, startCounter);
    testdbGetFieldEqual("doneCounter", DBF_LONG, ++doneCounter);
}

static
void testAsyncOutputs(dbCommon *async)
{
    const char * records[] = {
        "ao1", "bo1", "do1", "io1", "lo1", "lso1", "mo1", "so1", NULL,
    };
    const char ** rec = &records[0];

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    startCounter = doneCounter = 0;
    testdbPutFieldOk("startCounter", DBF_LONG, startCounter);
    testdbPutFieldOk("doneCounter", DBF_LONG, doneCounter);

    epicsEventTryWait(asyncEvent);
    epicsEventTryWait(doneEvent);

    while (*rec) {
        checkAsyncOutput(*rec++, async);
    }

    testDiag("============= Ending %s =============", EPICS_FUNCTION);
}

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(asyncSoftTest)
{
    dbCommon *async;

    testPlan(128);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("asyncSubr", (REGISTRYFUNCTION) asyncSubr);
    registryFunctionAdd("doneSubr", (REGISTRYFUNCTION) doneSubr);

    testdbReadDatabase("asyncSoftTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    async = testdbRecordPtr("async");
    asyncEvent = epicsEventCreate(epicsEventEmpty);
    doneEvent = epicsEventCreate(epicsEventEmpty);

    testAsynInputs(async);
    testAsyncOutputs(async);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
