/*************************************************************************\
* Copyright (c) 2026 Grass Wang
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbAccess.h"
#include "dbEvent.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsStdio.h"
#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void resetCounters(void)
{
    testdbPutFieldOk("firstCounter", DBF_DOUBLE, 0.0);
    testdbPutFieldOk("secondCounter", DBF_DOUBLE, 0.0);
    testdbPutFieldOk("forwardCounter", DBF_DOUBLE, 0.0);
}

static void processEvent(const char *record)
{
    char processField[32];

    epicsSnprintf(processField, sizeof(processField), "%s.PROC", record);
    testdbPutFieldOk(processField, DBF_LONG, 1);
    testSyncCallback();
}

static void testNamedEvent(void)
{
    resetCounters();
    testdbPutFieldOk("event.VAL", DBF_STRING, "FirstEvent");

    testDiag("event record posts its configured named event");
    processEvent("event");
    testdbGetFieldEqual("firstCounter", DBF_DOUBLE, 1.0);
    testdbGetFieldEqual("secondCounter", DBF_DOUBLE, 0.0);
    testdbGetFieldEqual("forwardCounter", DBF_DOUBLE, 1.0);
}

static void testValueRetarget(void)
{
    resetCounters();
    testdbPutFieldOk("event.VAL", DBF_STRING, "SecondEvent");

    testDiag("writing VAL retargets the event handle");
    processEvent("event");
    testdbGetFieldEqual("firstCounter", DBF_DOUBLE, 0.0);
    testdbGetFieldEqual("secondCounter", DBF_DOUBLE, 1.0);
    testdbGetFieldEqual("forwardCounter", DBF_DOUBLE, 1.0);
}

static void testSoftInput(void)
{
    resetCounters();

    testDiag("Soft Channel reads an event name from INP");
    testdbPutFieldOk("eventName", DBF_STRING, "FirstEvent");
    processEvent("softEvent");
    testdbGetFieldEqual("softEvent", DBF_STRING, "FirstEvent");
    testdbGetFieldEqual("softEvent.UDF", DBF_UCHAR, 0);
    testdbGetFieldEqual("firstCounter", DBF_DOUBLE, 1.0);
    testdbGetFieldEqual("secondCounter", DBF_DOUBLE, 0.0);

    testDiag("Soft Channel retargets when INP changes");
    testdbPutFieldOk("eventName", DBF_STRING, "SecondEvent");
    processEvent("softEvent");
    testdbGetFieldEqual("softEvent", DBF_STRING, "SecondEvent");
    testdbGetFieldEqual("softEvent.UDF", DBF_UCHAR, 0);
    testdbGetFieldEqual("firstCounter", DBF_DOUBLE, 1.0);
    testdbGetFieldEqual("secondCounter", DBF_DOUBLE, 1.0);
}

static void testValueMonitor(void)
{
    testMonitor *monitor;

    resetCounters();
    testdbPutFieldOk("event.VAL", DBF_STRING, "FirstEvent");
    monitor = testMonitorCreate("event", DBE_VALUE, 0);
    testMonitorSync(monitor);
    (void)testMonitorCount(monitor, 1);

    testDiag("event record posts a value monitor on each process");
    processEvent("event");
    testMonitorSync(monitor);
    testOk(testMonitorCount(monitor, 1) == 1,
        "first process posts one value monitor");

    processEvent("event");
    testMonitorSync(monitor);
    testOk(testMonitorCount(monitor, 1) == 1,
        "unchanged event name posts one value monitor");

    testMonitorDestroy(monitor);
}

MAIN(eventTest)
{
    testPlan(39);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("eventTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testNamedEvent();
    testValueRetarget();
    testSoftInput();
    testValueMonitor();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
