/*************************************************************************\
* Copyright (c) 2020 Joao Paulo Martins
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "dbLock.h"
#include "errlog.h"
#include "dbAccess.h"
#include "epicsMath.h"
#include "menuYesNo.h"

#include "longoutRecord.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_oopt_everytime(void){
    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write the same value two times */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* write two times with different values*/
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 18);

    /* Test if the counter_a was processed 4 times */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 4.0);

    // number of tests = 6
}

static void test_oopt_onchange(void){
    /* change OOPT to On Change */
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_On_Change);

    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write the same value two times */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter_a was processed only once */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    /* write two times with different values*/
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 18);

    /* Test if the counter_a was processed 1 + 2 times */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 3.0);

    //number of tests 8
}

static void test_oopt_whenzero(void){
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_When_Zero);

    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write zero two times */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);

    /* Test if the counter_a was processed twice */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 2.0);

    /* write two times with non-zero values*/
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 18);

    /* Test if the counter_a was still processed 2 times */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 2.0);

    //number of tests 8
}

static void test_oopt_whennonzero(void){
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_When_Non_zero);

    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write zero two times */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);

    /* Test if the counter_a was never processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 0.0);

    /* write two times with non-zero values*/
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 18);

    /* Test if the counter_a was still processed 2 times */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 2.0);

    //number of tests 8
}

static void test_oopt_when_transition_zero(void){
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_Transition_To_Zero);

    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write non-zero then zero */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);

    /* Test if the counter_a was processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    /* write another transition to zero */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);

    /* Test if the counter_a was processed once more */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 2.0);

    //number of tests 9
}

static void test_oopt_when_transition_nonzero(void){
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_Transition_To_Non_zero);

    /* write non-zero to start fresh */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* reset rec processing counter_a */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* write non-zero then zero */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 0);

    /* Test if the counter_a was never processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 0.0);

    /* write a transition to non-zero */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 18);
    
    /* Test if the counter_a was processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    //number of tests 8
}

static void test_changing_out_field(void){
    /* change OOPT to On Change */
    testdbPutFieldOk("longout_rec.OOPT", DBF_ENUM, longoutOOPT_On_Change);
    
    /* write an initial value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* reset rec processing counters */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);
    testdbPutFieldOk("counter_b.VAL", DBF_DOUBLE, 0.0);

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter was never processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 0.0);

    /* change the OUT link to another counter */
    testdbPutFieldOk("longout_rec.OUT", DBF_STRING, "counter_b.B PP");

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter was processed once */
    testdbGetFieldEqual("counter_b", DBF_DOUBLE, 1.0);

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter was not processed again */
    testdbGetFieldEqual("counter_b", DBF_DOUBLE, 1.0);

    /* Set option to write ON CHANGE even when the OUT link was changed */
    testdbPutFieldOk("longout_rec.OOCH", DBF_ENUM, menuYesNoNO);

    /* write an initial value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* reset rec processing counters */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);
    testdbPutFieldOk("counter_b.VAL", DBF_DOUBLE, 0.0);

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter_b was never processed */
    testdbGetFieldEqual("counter_b", DBF_DOUBLE, 0.0);

    /* change back the OUT link to counter_a */
    testdbPutFieldOk("longout_rec.OUT", DBF_STRING, "counter_a.B PP");

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter was never processed */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 0.0);

    /* write the same value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 16);

    /* Test if the counter was not processed again */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 0.0);

    /* write new value */
    testdbPutFieldOk("longout_rec.VAL", DBF_LONG, 17);

    /* Test if the counter was processed once */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    /* reset rec processing counters */
    testdbPutFieldOk("counter_a.VAL", DBF_DOUBLE, 0.0);

    /* test if record with OOPT == On Change will 
       write to output at its first process */
    testdbPutFieldOk("longout_rec2.VAL", DBF_LONG, 16);

    /* Test if the counter was processed once */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    /* write the same value */
    testdbPutFieldOk("longout_rec2.VAL", DBF_LONG, 16);

    /* Test if the counter was not processed again */
    testdbGetFieldEqual("counter_a", DBF_DOUBLE, 1.0);

    //number of tests 29
}

MAIN(longoutTest) {

    testPlan(6+8+8+8+9+8+29);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("longoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_oopt_everytime();
    test_oopt_onchange();
    test_oopt_whenzero();
    test_oopt_whennonzero();
    test_oopt_when_transition_zero();
    test_oopt_when_transition_nonzero();
    test_changing_out_field();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
