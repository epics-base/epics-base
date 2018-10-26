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
#include <dbCommon.h>
#include <dbAccess.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <iocsh.h>
#include "registryFunction.h"
#include <subRecord.h>

epicsEventId done;
static int waitFor;

static
long doneSubr(subRecord *prec)
{
    if (--waitFor <= 0)
        epicsEventMustTrigger(done);
    return 0;
}

void asyncproctest_registerRecordDeviceDriver(struct dbBase *);

MAIN(asyncproctest)
{
    testPlan(21);

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

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
