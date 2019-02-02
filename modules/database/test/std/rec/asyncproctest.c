/*************************************************************************\
* Copyright (c) 2018 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* This test covers some situations where asynchronous records are
 * dbProcess()'d while busy (PACT==1).
 */

#include <testMain.h>
#include <dbUnitTest.h>
#include <errlog.h>
#include <dbCommon.h>
#include <dbAccess.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <iocsh.h>
#include "registryFunction.h"
#include <subRecord.h>
#include <dbScan.h>

epicsEventId done;
static int waitFor;

static
long doneSubr(subRecord *prec)
{
    if (--waitFor <= 0)
        epicsEventMustTrigger(done);
    return 0;
}

static
void dummydone(void *usr, struct dbCommon* prec)
{
    epicsEventMustTrigger(done);
}

void asyncproctest_registerRecordDeviceDriver(struct dbBase *);

MAIN(asyncproctest)
{
    testPlan(27);

    done = epicsEventMustCreate(epicsEventEmpty);

    testdbPrepare();

    testdbReadDatabase("asyncproctest.dbd", NULL, NULL);
    asyncproctest_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("doneSubr", (REGISTRYFUNCTION) doneSubr);
    testdbReadDatabase("asyncproctest.db", NULL, "TPRO=0");

    dbAccessDebugPUTF = 1;

    testIocInitOk();
    testDiag("===== Chain 1 ======");

    waitFor = 2;
    testdbPutFieldOk("chain1.B", DBF_LONG, 6);
    testdbPutFieldOk("chain1.B", DBF_LONG, 7);

    if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
        testAbort("Processing timed out");

    testdbGetFieldEqual("chain1", DBF_LONG, 7);
    testdbGetFieldEqual("chain1.A", DBF_LONG, 2);

    testDiag("===== Chain 2 ======");

    waitFor = 2;
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 6);
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 7);

    if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
        testAbort("Processing timed out");

    testdbGetFieldEqual("chain2:1", DBF_LONG, 7);
    testdbGetFieldEqual("chain2:2", DBF_LONG, 7);
    testdbGetFieldEqual("chain2:1.A", DBF_LONG, 2);
    testdbGetFieldEqual("chain2:2.A", DBF_LONG, 2);

    testDiag("===== Chain 2 again ======");

    waitFor = 2;
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 6);
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 7);
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 8);

    if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
        testAbort("Processing timed out");

    testdbGetFieldEqual("chain2:1", DBF_LONG, 8);
    testdbGetFieldEqual("chain2:2", DBF_LONG, 8);
    testdbGetFieldEqual("chain2:1.A", DBF_LONG, 5);
    testdbGetFieldEqual("chain2:2.A", DBF_LONG, 4);

    testDiag("===== Chain 3 ======");

    waitFor = 2;
    testdbPutFieldOk("chain3.B", DBF_LONG, 6);
    testdbPutFieldOk("chain3.B", DBF_LONG, 7);

    if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
        testAbort("Processing timed out");

    testdbGetFieldEqual("chain3", DBF_LONG, 7);
    testdbGetFieldEqual("chain3.A", DBF_LONG, 2);

    testDiag("===== Chain 4 ======");

    {
        dbCommon *dummy=testdbRecordPtr("chain4_dummy");

        testdbPutFieldOk("chain4_pos.PROC", DBF_LONG, 0);

        /* sync once queue to wait for any queued RPRO */
        scanOnceCallback(dummy, dummydone, NULL);

        if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
            testAbort("Processing timed out");

        testdbGetFieldEqual("chain4_pos", DBF_SHORT, 1);
        testdbGetFieldEqual("chain4_rel", DBF_SHORT, 1);
        testdbGetFieldEqual("chain4_lim", DBF_SHORT, 1);
    }

    testDiag("===== Chain 5 ======");

    {
        dbCommon *dummy=testdbRecordPtr("chain4_dummy");

        testdbPutFieldOk("chain5_cnt.PROC", DBF_LONG, 0);

        /* sync once queue to wait for any queued RPRO */
        scanOnceCallback(dummy, dummydone, NULL);

        if (epicsEventWaitWithTimeout(done, 10.0) != epicsEventOK)
            testAbort("Processing timed out");

        testdbGetFieldEqual("chain5_cnt", DBF_SHORT, 1);
    }

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
