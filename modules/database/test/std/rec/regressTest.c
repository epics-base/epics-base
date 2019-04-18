/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

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

/* lp:1824277 Regression in calcout, setting links at runtime */
static void
testSpecialLinks(void)
{
    testDiag("In testSpecialLinks()");

    startRegressTestIoc("regressCalcout.db");

    testdbPutFieldOk("cout.INPA", DBF_STRING, "10");
    testdbGetFieldEqual("cout.A", DBF_LONG, 10);
    testdbGetFieldEqual("cout.INAV", DBF_LONG, calcoutINAV_CON);
    testdbPutFieldOk("cout.INPB", DBF_STRING, "{\"const\":20}");
    testdbGetFieldEqual("cout.B", DBF_LONG, 20);
    testdbGetFieldEqual("cout.INBV", DBF_LONG, calcoutINAV_CON);
    testdbPutFieldOk("cout.INPC", DBF_STRING, "cout.A");
    testdbGetFieldEqual("cout.C", DBF_LONG, 99);
    testdbGetFieldEqual("cout.INCV", DBF_LONG, calcoutINAV_LOC);
    testdbPutFieldOk("cout.INPD", DBF_STRING, "no-such-pv");
    testdbGetFieldEqual("cout.D", DBF_LONG, 99);
    testdbGetFieldEqual("cout.INDV", DBF_LONG, calcoutINAV_EXT_NC);

    eltc(0);
    testdbPutFieldOk("ain.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "ain.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("bin.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "bin.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("iin.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "iin.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("lin.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "lin.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("min.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "min.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("din.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "din.INP", DBF_STRING, "{\"const\":1}");
    testdbPutFieldOk("sin.INP", DBF_STRING, "cout");
    testdbPutFieldFail(S_db_badField, "sin.INP", DBF_STRING, "{\"const\":1}");
    eltc(1);

    testIocShutdownOk();
    testdbCleanup();
}


MAIN(regressTest)
{
    testPlan(60);
    testArrayLength1();
    testHexConstantLinks();
    testLinkMS();
    testCADisconn();
    testSpecialLinks();
    return testDone();
}
