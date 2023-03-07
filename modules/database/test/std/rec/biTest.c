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

MAIN(biTest) {

    testPlan(6+6+11+13);

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

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
