/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <errlog.h>

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
 * https://bugs.launchpad.net/epics-base/+bug/1699445
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


MAIN(regressTest)
{
    testPlan(31);
    testArrayLength1();
    testHexConstantLinks();
    testLinkMS();
    return testDone();
}
