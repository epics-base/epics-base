/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#define EPICS_DBCA_PRIVATE_API

#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <errlog.h>
#include <alarm.h>

#include <calcoutRecord.h>
#include <waveformRecord.h>

/*
 * Tests for specific regressions
 */

void regressTest_registerRecordDeviceDriver(struct dbBase *);

static
void startRegressTestIoc(const char *dbfile)
{
    testDiag("Testing with %s", dbfile);

    testdbPrepare();
    testdbReadDatabase("regressTest.dbd", NULL, NULL);
    regressTest_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase(dbfile, NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
}

/*
 * https://bugs.launchpad.net/epics-base/+bug/1577108
 */
static
void testArrayLength1(void)
{
    waveformRecord *precwf;
    calcoutRecord  *precco;
    double *pbuf;

    startRegressTestIoc("regressArray1.db");

    precwf = (waveformRecord*)testdbRecordPtr("wf");
    precco = (calcoutRecord*)testdbRecordPtr("co");

    dbScanLock((dbCommon*)precwf);
    pbuf = (double*)precwf->bptr;
    dbScanUnlock((dbCommon*)precwf);

    testdbPutFieldOk("wf", DBF_DOUBLE, 2.0);

    dbScanLock((dbCommon*)precwf);
    testOk(precwf->nord==1, "wf.NORD = %u == 1", (unsigned)precwf->nord);
    testOk(pbuf[0]==2.0, "wf.VAL[0] = %f == 2.0", pbuf[0]);
    dbScanUnlock((dbCommon*)precwf);

    dbScanLock((dbCommon*)precco);
    testOk(precco->a==2.0, "co.A = %f == 2.0", precco->a);
    dbScanUnlock((dbCommon*)precco);

    testdbGetFieldEqual("co", DBF_DOUBLE, 2.0);

    testIocShutdownOk();

    testdbCleanup();
}

/*
 * https://bugs.launchpad.net/epics-base/+bug/1699445 and 1887981
 *
 * also https://github.com/epics-base/epics-base/issues/284
 */
static
void testHexConstantLinks(void)
{
    startRegressTestIoc("regressHex.db");

    testdbGetFieldEqual("ai1", DBR_LONG, 0x10);
    testdbGetFieldEqual("li1", DBR_LONG, 0x10);
    testdbGetFieldEqual("mi1", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.A", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.B", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.C", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.D", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.E", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.F", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.G", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.H", DBR_LONG, 0x10);

    testdbPutFieldOk("ai1.PROC", DBR_LONG, 1);
    testdbPutFieldOk("li1.PROC", DBR_LONG, 1);
    testdbPutFieldOk("as1.PROC", DBR_LONG, 1);

    testdbGetFieldEqual("ai1", DBR_LONG, 0x10);
    testdbGetFieldEqual("li1", DBR_LONG, 0x10);
    testdbGetFieldEqual("mi1", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.A", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.B", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.C", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.D", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.E", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.F", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.G", DBR_LONG, 0x10);
    testdbGetFieldEqual("as1.H", DBR_LONG, 0x10);

    testIocShutdownOk();
    testdbCleanup();
}

static
void testLinkMS(void)
{
    startRegressTestIoc("regressLinkMS.db");

    testdbPutFieldOk("alarm", DBF_DOUBLE, 0.5);
    testdbGetFieldEqual("latch", DBR_DOUBLE, 0.5);
    testdbGetFieldEqual("latch.SEVR", DBR_LONG, 0);

    testdbPutFieldOk("alarm", DBF_DOUBLE, 1.5);
    testdbGetFieldEqual("latch", DBR_DOUBLE, 1.5);
    testdbGetFieldEqual("latch.SEVR", DBR_LONG, 1);

    testdbPutFieldOk("alarm", DBF_DOUBLE, 0.5);
    testdbGetFieldEqual("latch", DBR_DOUBLE, 0.5);
    testdbGetFieldEqual("latch.SEVR", DBR_LONG, 0);

    testdbPutFieldOk("alarm", DBF_DOUBLE, 2.5);
    testdbGetFieldEqual("latch", DBR_DOUBLE, 2.5);
    testdbGetFieldEqual("latch.SEVR", DBR_LONG, 2);

    testdbPutFieldOk("alarm", DBF_DOUBLE, 0.5);
    testdbGetFieldEqual("latch", DBR_DOUBLE, 0.5);
    testdbGetFieldEqual("latch.SEVR", DBR_LONG, 0);

    testIocShutdownOk();
    testdbCleanup();
}

/* lp:1798855 disconnected CA link must alarm */
static
void testCADisconn(void)
{
    testDiag("In testCADisconn()");

    startRegressTestIoc("badCaLink.db");

    testdbPutFieldFail(-1, "ai:disconn.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("ai:disconn.SEVR", DBF_LONG, INVALID_ALARM);
    testdbGetFieldEqual("ai:disconn.STAT", DBF_LONG, LINK_ALARM);

    testIocShutdownOk();
    testdbCleanup();
}

/* https://github.com/epics-base/epics-base/issues/194 */
static
void testLongCalc(void)
{
    const char small[] = "0.0000000000000000000000000000000000000001";

    startRegressTestIoc("regressLongCalc.db");

    testdbGetFieldEqual("test_calc.CALC", DBF_STRING, "RNDM*100");

    testdbPutArrFieldOk("test_calc.CALC$", DBF_CHAR, 4, "300\0");
    testdbGetFieldEqual("test_calc.CALC", DBF_STRING, "300");
    testdbPutFieldOk("test_calc.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calc", DBF_DOUBLE, 300.0);

    testdbPutArrFieldOk("test_calc.CALC$", DBF_CHAR, sizeof(small), small);
    testdbGetFieldEqual("test_calc.CALC", DBF_STRING, "0.0000000000000000000000000000000000000");
    testdbPutFieldOk("test_calc.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calc", DBF_DOUBLE, 1e-40);

    testIocShutdownOk();
    testdbCleanup();
}

/* https://github.com/epics-base/epics-base/issues/183 */
static
void testLinkSevr(void)
{
    dbChannel *chan;

    startRegressTestIoc("regressLinkSevr.db");

    /* wait for CA links to connect and receive an initial update */
    testdbCaWaitForUpdateCount(dbGetDevLink(testdbRecordPtr("si2")), 1);
    testdbCaWaitForUpdateCount(dbGetDevLink(testdbRecordPtr("li2")), 1);

    chan = dbChannelCreate("ai.SEVR");
    if(!chan)
        testAbort("Can't create channel for ai.SEVR");
    testOk1(!dbChannelOpen(chan));

#define testType(FN, TYPE) testOk(FN(chan)==TYPE, #FN "() -> (%d) == " #TYPE " (%d)", FN(chan), TYPE)
    testType(dbChannelExportType, DBF_ENUM);
    testType(dbChannelFieldType, DBF_MENU);
    testType(dbChannelFinalFieldType, DBF_ENUM);
#undef testType

    dbChannelDelete(chan);

    testdbGetFieldEqual("ai.SEVR", DBF_LONG, INVALID_ALARM);
    testdbGetFieldEqual("ai.SEVR", DBF_STRING, "INVALID");

    testdbPutFieldOk("si1.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("si1", DBF_STRING, "INVALID");
    testdbGetFieldEqual("li1", DBF_LONG, INVALID_ALARM);
    testTodoBegin("Not working");
    testdbGetFieldEqual("si2", DBF_STRING, "INVALID");
    testTodoEnd();
    testdbGetFieldEqual("li2", DBF_LONG, INVALID_ALARM);

    testIocShutdownOk();
    testdbCleanup();
}

MAIN(regressTest)
{
    testPlan(68);
    testArrayLength1();
    testHexConstantLinks();
    testLinkMS();
    testCADisconn();
    testLongCalc();
    testLinkSevr();
    return testDone();
}
