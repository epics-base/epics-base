#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuAlarmSevr.h"
#include "menuIvoa.h"

#include "calcoutRecord.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);


static void test_sum(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double sum = val1 + val2;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1+val2);
}

static void test_oopt_on_change(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double sum = val1 + val2;

    double val3 = 0.0;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");

    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_On_Change);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);   

    // test if value did not change after processing
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val3);
}

static void test_oopt_every_time(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double sum = val1 + val2;

    double val3 = 0.0;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");

    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);

    //test if value changes after processing
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);
}

static void test_oopt_when_zero(void){
    double val1 = 0.0;
    double val2 = 1.0;

    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val2);
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_When_Zero);
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val2);

    // test if value changes after processing
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1);
}

static void test_oopt_when_non_zero(void){
    double val1 = 1.0;
    double val2 = 0.0;

    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val2);
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_When_Non_zero);
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val2);

    // test if value changes after processing
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1);
}

static void test_oopt_transition_to_zero(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double diff = val1 - val2;
    double val3 = 0.0;

    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Transition_To_Zero);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A-B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val3);

    //test if value changes after processing
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.VAL", DBF_LONG, 0);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, 0.0);
}


static void test_oopt_transition_to_non_zero(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double diff = val1 - val2;
    double val3 = 0.0;

    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val1);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Transition_To_Non_zero);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A-B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val3);

    //test if value changes after processing
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val2);
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, 1.0);
    testdbGetFieldEqual("test_out.VAL", DBF_LONG, 1);
}


MAIN(calcoutTest){
    testPlan(9+12+12+11+11+14+14);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("calcoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_sum();
    testPass("PASS!");
   
    test_oopt_on_change();
    test_oopt_every_time();
    test_oopt_when_zero();
    test_oopt_when_non_zero();
    test_oopt_transition_to_zero();
    test_oopt_transition_to_non_zero();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
