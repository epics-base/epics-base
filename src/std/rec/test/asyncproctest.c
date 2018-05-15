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
#include <epicsThread.h>
#include <iocsh.h>

void asyncproctest_registerRecordDeviceDriver(struct dbBase *);

MAIN(asyncproctest)
{
    testPlan(0);

    testdbPrepare();

    testdbReadDatabase("asyncproctest.dbd", NULL, NULL);
    asyncproctest_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("asyncproctest.db", NULL, "TPRO=1");

    testIocInitOk();
    testDiag("===== Chain 1 ======");

    testdbPutFieldOk("chain1.B", DBF_LONG, 6);
    testdbPutFieldOk("chain1.B", DBF_LONG, 7);

    epicsThreadSleep(1.0);

    testdbGetFieldEqual("chain1", DBF_LONG, 7);
    testdbGetFieldEqual("chain1.A", DBF_LONG, 2);

    testDiag("===== Chain 2 ======");

    testdbPutFieldOk("chain2:1.B", DBF_LONG, 6);
    testdbPutFieldOk("chain2:1.B", DBF_LONG, 7);

    epicsThreadSleep(1.0);

    testdbGetFieldEqual("chain2:1", DBF_LONG, 7);
    testdbGetFieldEqual("chain2:2", DBF_LONG, 7);
    testdbGetFieldEqual("chain2:1.A", DBF_LONG, 2);
    testdbGetFieldEqual("chain2:2.A", DBF_LONG, 2);

    testDiag("===== Chain 3 ======");

    testdbPutFieldOk("chain3.B", DBF_LONG, 6);
    testdbPutFieldOk("chain3.B", DBF_LONG, 7);

    epicsThreadSleep(1.0);

    testdbGetFieldEqual("chain3", DBF_LONG, 7);
    testdbGetFieldEqual("chain3.A", DBF_LONG, 2);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
