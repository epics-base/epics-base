/*************************************************************************\
* Copyright (c) 2023 Karl Vestin
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuAlarmSevr.h"
#include "menuScan.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_soft_input(void){
    /* set soft channel */
    testdbPutFieldOk("test_bi_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bi_rec.INP", DBF_STRING, "test_bi_link_rec.VAL");
    testdbPutFieldOk("test_bi_link_rec.FLNK", DBF_STRING, "test_bi_rec");

    /* set VAL to on linked record */
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, TRUE);

    /* verify that this record VAL is updated but RVAL is not */
    testdbGetFieldEqual("test_bi_rec.VAL", DBF_SHORT, TRUE);
    testdbGetFieldEqual("test_bi_rec.RVAL", DBF_SHORT, FALSE);

    // number of tests = 6
}

static void test_raw_soft_input(void){
    /* set soft channel */
    testdbPutFieldOk("test_bi_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_bi_rec.INP", DBF_STRING, "test_bi_link_rec.VAL");
    testdbPutFieldOk("test_bi_link_rec.FLNK", DBF_STRING, "test_bi_rec");

    /* set VAL to on linked record */
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, TRUE);

    /* verify that this record RVAL and VAL are updated */
    testdbGetFieldEqual("test_bi_rec.VAL", DBF_SHORT, TRUE);
    testdbGetFieldEqual("test_bi_rec.RVAL", DBF_SHORT, TRUE);

    // number of tests = 6
}

static void test_operator_display(void){
    /* set operator display parameters */
    testdbPutFieldOk("test_bi_rec.ZNAM", DBF_STRING, "ZNAM_TEST");
    testdbPutFieldOk("test_bi_rec.ONAM", DBF_STRING, "ONAM_TEST");
    testdbPutFieldOk("test_bi_rec.DESC", DBF_STRING, "DESC_TEST");

    /* verify operator display parameters */
    testdbGetFieldEqual("test_bi_rec.ZNAM", DBF_STRING, "ZNAM_TEST");
    testdbGetFieldEqual("test_bi_rec.ONAM", DBF_STRING, "ONAM_TEST");
    testdbGetFieldEqual("test_bi_rec.NAME", DBF_STRING, "test_bi_rec");
    testdbGetFieldEqual("test_bi_rec.DESC", DBF_STRING, "DESC_TEST");

    /* verify conversion */
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, TRUE);
    testdbGetFieldEqual("test_bi_rec.VAL", DBF_STRING, "ONAM_TEST");
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, FALSE);
    testdbGetFieldEqual("test_bi_rec.VAL", DBF_STRING, "ZNAM_TEST");

    // number of tests = 11
}

static void test_alarm(void){
    /* set soft channel */
    testdbPutFieldOk("test_bi_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bi_rec.INP", DBF_STRING, "test_bi_link_rec.VAL");
    testdbPutFieldOk("test_bi_link_rec.FLNK", DBF_STRING, "test_bi_rec");

    /* set start VAL to FALSE*/
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, FALSE);

    /* set alarm parameters */
    testdbPutFieldOk("test_bi_rec.ZSV", DBF_SHORT, menuAlarmSevrNO_ALARM);
    testdbPutFieldOk("test_bi_rec.OSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_bi_rec.COSV", DBF_SHORT, menuAlarmSevrINVALID);

    /* verify alarm status is NO_ALARM*/
    testdbGetFieldEqual("test_bi_rec.SEVR", DBF_SHORT, menuAlarmSevrNO_ALARM);

    /* set ZSV to MAJOR and verify that SEVR is now MAJOR */
    testdbPutFieldOk("test_bi_rec.ZSV", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbGetFieldEqual("test_bi_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set VAL to 1 on linked record and verify that COSV now sets the SEVR to INVALID */
    testdbPutFieldOk("test_bi_link_rec.VAL", DBF_SHORT, TRUE);
    testdbGetFieldEqual("test_bi_rec.SEVR", DBF_SHORT, menuAlarmSevrINVALID);

    /* verify LAML */
    testdbGetFieldEqual("test_bi_rec.LALM", DBF_SHORT, TRUE);

    // number of tests = 13
}

static void test_aftc(void){
    const double aftc = 3.0;
    testMonitor* test_mon = NULL;
    epicsTimeStamp startTime;
    epicsTimeStamp endTime;
    double diffTime = 0;

    /* set soft channel */
    testdbPutFieldOk("test_bi_rec2.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bi_rec2.INP", DBF_STRING, "test_bi_link_rec2.VAL");
    testdbPutFieldOk("test_bi_link_rec2.FLNK", DBF_STRING, "test_bi_rec2");

    /* set alarm parameters */
    testdbPutFieldOk("test_bi_rec2.ZSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_bi_rec2.OSV", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set start VAL to FALSE (NO_ALARM) */
    testdbPutFieldOk("test_bi_link_rec2.VAL", DBF_SHORT, FALSE);

    /* test AFTC using a monitor and time stamps */
    testdbPutFieldOk("test_bi_rec2.AFTC", DBF_DOUBLE, aftc);
    testdbPutFieldOk("test_bi_rec2.SCAN", DBF_SHORT, menuScan_1_second);

    /* set VAL to TRUE (MAJOR alarm) */
    testdbPutFieldOk("test_bi_link_rec2.VAL", DBF_SHORT, TRUE);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_bi_rec2.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status */
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now MAJOR */
    testdbGetFieldEqual("test_bi_rec2.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set VAL back to FALSE (NO_ALARM) */
    testdbPutFieldOk("test_bi_link_rec2.VAL", DBF_SHORT, FALSE);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_bi_rec2.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status */
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now NO_ALARM */
    testdbGetFieldEqual("test_bi_rec2.SEVR", DBF_SHORT, menuAlarmSevrMINOR);

    /* Verify that time is at least equal to configured aftc */
    diffTime = epicsTimeDiffInSeconds(&endTime, &startTime);
    testOk(diffTime >= aftc, "AFTC time %lf", diffTime);

    // number of tests = 13
}

MAIN(biTest) {

    testPlan(6+6+11+13+13);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("biTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    test_soft_input();
    test_raw_soft_input();
    test_operator_display();
    test_alarm();
    test_aftc();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
