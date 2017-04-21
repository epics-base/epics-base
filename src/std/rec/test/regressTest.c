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
 * Test the some identified regressions
 */

void regressTest_registerRecordDeviceDriver(struct dbBase *);

/*
 * https://bugs.launchpad.net/epics-base/+bug/1577108
 */
static
void testArrayLength1(void)
{
    waveformRecord *precwf;
    calcoutRecord  *precco;
    double *pbuf;

    testdbPrepare();

    testdbReadDatabase("regressTest.dbd", NULL, NULL);

    regressTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("regressArray1.db", NULL, NULL);

    precwf = (waveformRecord*)testdbRecordPtr("wf");
    precco = (calcoutRecord*)testdbRecordPtr("co");

    eltc(0);
    testIocInitOk();
    eltc(1);

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

MAIN(regressTest)
{
    testPlan(5);
    testArrayLength1();
    return testDone();
}
