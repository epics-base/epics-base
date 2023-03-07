/*************************************************************************\
* Copyright (c) 2023 Karl Vestin
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <unistd.h>

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuAlarmSevr.h"
#include "menuIvoa.h"


#include "boRecord.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_soft_output(void){
    /* set soft channel */
    testdbPutFieldOk("test_bo_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bo_rec.OUT", DBF_STRING, "test_bo_link_rec");

    /* set VAL to process record */
    testdbPutFieldOk("test_bo_rec.VAL", DBF_SHORT, TRUE);

    /* verify that OUT record is updated */
    testdbGetFieldEqual("test_bo_link_rec.VAL", DBF_SHORT, TRUE);

    // number of tests = 4
}

static void test_high(void){
    const int high_time = 2;
    const int high_wait_time = high_time + 1;

    /* set soft channel */
    testdbPutFieldOk("test_bo_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bo_rec.OUT", DBF_STRING, "test_bo_link_rec");

    /* set HIGH to 2 seconds */
    testdbPutFieldOk("test_bo_rec.HIGH", DBF_SHORT, high_time);

    /* set VAL to process record */
    testdbPutFieldOk("test_bo_rec.VAL", DBF_SHORT, TRUE);

    /* verify that OUT record is updated */
    testdbGetFieldEqual("test_bo_link_rec.VAL", DBF_SHORT, TRUE);

    /* Wait and verfiy that both records are set back to 0 */
    sleep(high_wait_time);
    testdbGetFieldEqual("test_bo_rec.VAL", DBF_SHORT, FALSE);
    testdbGetFieldEqual("test_bo_link_rec.VAL", DBF_SHORT, FALSE);
    // number of tests = 7
}

static void test_operator_display(void){
    /* set operator display parameters */
    testdbPutFieldOk("test_bo_rec.ZNAM", DBF_STRING, "ZNAM_TEST");
    testdbPutFieldOk("test_bo_rec.ONAM", DBF_STRING, "ONAM_TEST");
    testdbPutFieldOk("test_bo_rec.DESC", DBF_STRING, "DESC_TEST");

    /* verify operator display parameters */
    testdbGetFieldEqual("test_bo_rec.ZNAM", DBF_STRING, "ZNAM_TEST");   
    testdbGetFieldEqual("test_bo_rec.ONAM", DBF_STRING, "ONAM_TEST"); 
    testdbGetFieldEqual("test_bo_rec.NAME", DBF_STRING, "test_bo_rec");
    testdbGetFieldEqual("test_bo_rec.DESC", DBF_STRING, "DESC_TEST");

    // number of tests = 7
}

static void test_alarm(void){
    /* set soft channel */
    testdbPutFieldOk("test_bo_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_bo_rec.OUT", DBF_STRING, "test_bo_link_rec");

    /* Set start VAL to FALSE*/
    testdbPutFieldOk("test_bo_rec.VAL", DBF_SHORT, FALSE);

    /* set alarm parameters */
    testdbPutFieldOk("test_bo_rec.ZSV", DBF_SHORT, menuAlarmSevrNO_ALARM);
    testdbPutFieldOk("test_bo_rec.OSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_bo_rec.COSV", DBF_SHORT, menuAlarmSevrINVALID);
    testdbPutFieldOk("test_bo_rec.IVOA", DBF_SHORT, menuIvoaSet_output_to_IVOV);
    testdbPutFieldOk("test_bo_rec.IVOV", DBF_SHORT, FALSE);
    
    /* Verify alarm status is NO_ALARM*/
    testdbGetFieldEqual("test_bo_rec.SEVR", DBF_SHORT, menuAlarmSevrNO_ALARM);

    /* Set ZSV to MAJOR and verify that SEVR is now MAJOR */
    testdbPutFieldOk("test_bo_rec.ZSV", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbGetFieldEqual("test_bo_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* Set VAL to 1 and verify that COSV now sets the SEVR to INVALID and in turn triggers the IVOV on output */
    testdbPutFieldOk("test_bo_link_rec.VAL", DBF_SHORT, TRUE);
    testdbPutFieldOk("test_bo_rec.VAL", DBF_SHORT, TRUE);
    testdbGetFieldEqual("test_bo_rec.SEVR", DBF_SHORT, menuAlarmSevrINVALID);
    testdbGetFieldEqual("test_bo_link_rec.VAL", DBF_SHORT, FALSE);

    // number of tests = 15
}

MAIN(boTest) {

    testPlan(4+7+7+15);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("boTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_soft_output();
    test_high();
    test_operator_display();
    test_alarm();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
