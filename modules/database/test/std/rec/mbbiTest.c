/*************************************************************************\
* Copyright (c) 2026 Edmund Blomley
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
    testdbPutFieldOk("test_mbbi_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_mbbi_rec.INP", DBF_STRING, "test_mbbi_link_rec.VAL");
    testdbPutFieldOk("test_mbbi_link_rec.FLNK", DBF_STRING, "test_mbbi_rec");

    /* set VAL to on linked record */
    testdbPutFieldOk("test_mbbi_link_rec.VAL", DBF_SHORT, 1);

    /* verify that this record VAL is updated but RVAL is not */
    testdbGetFieldEqual("test_mbbi_rec.VAL", DBF_SHORT, 1);
    testdbGetFieldEqual("test_mbbi_rec.RVAL", DBF_SHORT, 0);

    // number of tests = 6
}

static void test_operator_display(void){
    /* set operator display parameters */
    testdbPutFieldOk("test_mbbi_rec.ZRST", DBF_STRING, "ZRST_TEST");
    testdbPutFieldOk("test_mbbi_rec.ONST", DBF_STRING, "ONST_TEST");
    testdbPutFieldOk("test_mbbi_rec.TWST", DBF_STRING, "TWST_TEST");
    testdbPutFieldOk("test_mbbi_rec.THST", DBF_STRING, "THST_TEST");
    testdbPutFieldOk("test_mbbi_rec.FRST", DBF_STRING, "FRST_TEST");
    testdbPutFieldOk("test_mbbi_rec.FVST", DBF_STRING, "FVST_TEST");
    testdbPutFieldOk("test_mbbi_rec.SXST", DBF_STRING, "SXST_TEST");
    testdbPutFieldOk("test_mbbi_rec.SVST", DBF_STRING, "SVST_TEST");
    testdbPutFieldOk("test_mbbi_rec.EIST", DBF_STRING, "EIST_TEST");
    testdbPutFieldOk("test_mbbi_rec.NIST", DBF_STRING, "NIST_TEST");
    testdbPutFieldOk("test_mbbi_rec.TEST", DBF_STRING, "TEST_TEST");
    testdbPutFieldOk("test_mbbi_rec.ELST", DBF_STRING, "ELST_TEST");
    testdbPutFieldOk("test_mbbi_rec.TVST", DBF_STRING, "TVST_TEST");
    testdbPutFieldOk("test_mbbi_rec.TTST", DBF_STRING, "TTST_TEST");
    testdbPutFieldOk("test_mbbi_rec.FTST", DBF_STRING, "FTST_TEST");
    testdbPutFieldOk("test_mbbi_rec.FFST", DBF_STRING, "FFST_TEST");
    testdbPutFieldOk("test_mbbi_rec.DESC", DBF_STRING, "DESC_TEST");

    /* verify operator display parameters */
    testdbGetFieldEqual("test_mbbi_rec.ZRST", DBF_STRING, "ZRST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.ONST", DBF_STRING, "ONST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.TWST", DBF_STRING, "TWST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.THST", DBF_STRING, "THST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.FRST", DBF_STRING, "FRST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.FVST", DBF_STRING, "FVST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.SXST", DBF_STRING, "SXST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.SVST", DBF_STRING, "SVST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.EIST", DBF_STRING, "EIST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.NIST", DBF_STRING, "NIST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.TEST", DBF_STRING, "TEST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.ELST", DBF_STRING, "ELST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.TVST", DBF_STRING, "TVST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.TTST", DBF_STRING, "TTST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.FTST", DBF_STRING, "FTST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.FFST", DBF_STRING, "FFST_TEST");
    testdbGetFieldEqual("test_mbbi_rec.NAME", DBF_STRING, "test_mbbi_rec");
    testdbGetFieldEqual("test_mbbi_rec.DESC", DBF_STRING, "DESC_TEST");

    /* verify conversion */
    testdbPutFieldOk("test_mbbi_link_rec.VAL", DBF_SHORT, 1);
    testdbGetFieldEqual("test_mbbi_rec.VAL", DBF_STRING, "ONST_TEST");
    testdbPutFieldOk("test_mbbi_link_rec.VAL", DBF_SHORT, 0);
    testdbGetFieldEqual("test_mbbi_rec.VAL", DBF_STRING, "ZRST_TEST");

    // number of tests = 39
}

static void test_alarm(void){
    /* set soft channel */
    testdbPutFieldOk("test_mbbi_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_mbbi_rec.INP", DBF_STRING, "test_mbbi_link_rec.VAL");
    testdbPutFieldOk("test_mbbi_link_rec.FLNK", DBF_STRING, "test_mbbi_rec");
    testdbPutFieldOk("test_mbbi_rec.ZRST", DBF_STRING, "ZRST_TEST");
    testdbPutFieldOk("test_mbbi_rec.ONST", DBF_STRING, "ONST_TEST");
    testdbPutFieldOk("test_mbbi_rec.ZRVL", DBF_STRING, "0");
    testdbPutFieldOk("test_mbbi_rec.ONVL", DBF_STRING, "1");

    /* set start VAL to 0*/
    testdbPutFieldOk("test_mbbi_link_rec.VAL", DBF_SHORT, 0);

    /* set alarm parameters */
    testdbPutFieldOk("test_mbbi_rec.ZRSV", DBF_SHORT, menuAlarmSevrNO_ALARM);
    testdbPutFieldOk("test_mbbi_rec.ONSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_mbbi_rec.COSV", DBF_SHORT, menuAlarmSevrINVALID);

    /* verify alarm status is NO_ALARM*/
    testdbGetFieldEqual("test_mbbi_rec.SEVR", DBF_SHORT, menuAlarmSevrNO_ALARM);

    /* set ZRSV to MAJOR and verify that SEVR is now MAJOR */
    testdbPutFieldOk("test_mbbi_rec.ZRSV", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbGetFieldEqual("test_mbbi_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set VAL to 1 on linked record and verify that COSV now sets the SEVR to INVALID */
    testdbPutFieldOk("test_mbbi_link_rec.VAL", DBF_SHORT, 1);
    testdbGetFieldEqual("test_mbbi_rec.SEVR", DBF_SHORT, menuAlarmSevrINVALID);

    /* verify LAML */
    testdbGetFieldEqual("test_mbbi_rec.LALM", DBF_SHORT, 1);

    // number of tests = 17
}

static void test_aftc(void){
    const double aftc = 3.0;
    testMonitor* test_mon = NULL;
    epicsTimeStamp startTime;
    epicsTimeStamp endTime;
    double diffTime = 0;

    /* set soft channel */
    testdbPutFieldOk("test_mbbi_rec2.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_mbbi_rec2.INP", DBF_STRING, "test_mbbi_link_rec2.VAL");
    testdbPutFieldOk("test_mbbi_link_rec2.FLNK", DBF_STRING, "test_mbbi_rec2");

    /* set alarm parameters */
    testdbPutFieldOk("test_mbbi_rec2.ZRSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_mbbi_rec2.ONSV", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set start VAL to FALSE (NO_ALARM) */
    testdbPutFieldOk("test_mbbi_link_rec2.VAL", DBF_SHORT, FALSE);

    /* test AFTC using a monitor and time stamps */
    testdbPutFieldOk("test_mbbi_rec2.AFTC", DBF_DOUBLE, aftc);
    testdbPutFieldOk("test_mbbi_rec2.SCAN", DBF_SHORT, menuScan_1_second);

    /* set VAL to TRUE (MAJOR alarm) */
    testdbPutFieldOk("test_mbbi_link_rec2.VAL", DBF_SHORT, TRUE);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_mbbi_rec2.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status */
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now MAJOR */
    testdbGetFieldEqual("test_mbbi_rec2.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set VAL back to FALSE (NO_ALARM) */
    testdbPutFieldOk("test_mbbi_link_rec2.VAL", DBF_SHORT, FALSE);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_mbbi_rec2.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status */
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now NO_ALARM */
    testdbGetFieldEqual("test_mbbi_rec2.SEVR", DBF_SHORT, menuAlarmSevrMINOR);

    /* Verify that time is at least equal to configured aftc */
    diffTime = epicsTimeDiffInSeconds(&endTime, &startTime);
    testOk(diffTime >= aftc, "AFTC time %lf", diffTime);

    // number of tests = 13
}

MAIN(mbbiTest) {

    testPlan(6+39+17+13);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("mbbiTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    test_soft_input();
    test_operator_display();
    test_alarm();
    test_aftc();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
