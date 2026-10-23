/*************************************************************************\
* Copyright (c) 2026 Marnelli Martinez - Oak Ridge Natl Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdio.h>

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuAlarmSevr.h"
#include "menuIvoa.h"

#include "calcoutRecord.h"

#define NUM_OF_INPUTS 21
#define NUM_OF_PATTERNS 5

/* INP field to test */
static const char t_INPField[NUM_OF_INPUTS][2] = {
    {"A"}, {"B"}, {"C"}, {"D"}, {"E"},
    {"F"}, {"G"}, {"H"}, {"I"}, {"J"},
    {"K"}, {"L"}, {"M"}, {"N"}, {"O"},
    {"P"}, {"Q"}, {"R"}, {"S"}, {"T"},
    {"U"}
};

static double t_ValuePatterns[NUM_OF_PATTERNS][2] = {
    { 0.0, 0.0 },    /* no change in value */
    { 0.5, 0.5 },    /* non-zero, but no change */
    { 1.0, 1.5 },    /* value change */
    { 0.5, 0.0 },    /* transition to zero */
    { 0.0, 0.5 }     /* transition to non-zero */
};

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

/* index for INP field to test */
static int i;

static void test_sum_grp(const char field1[2], const char field2[2], const char field3[2], int n){
    double val1 = n + 1.0;
    double val2 = n + 2.0;
    double val3 = n + 0.5;
    double sum = val1 + val2 + val3;

    char inp1[20] = "";
    char inp2[20] = "";
    char inp3[20] = "";
    char calc[20] = "";

    /* create INP links and CALC formula */
    snprintf(inp1, sizeof(inp1), "test_calcout.INP%s", field1);
    snprintf(inp2, sizeof(inp2), "test_calcout.INP%s", field2);
    snprintf(inp3, sizeof(inp3), "test_calcout.INP%s", field3);
    snprintf(calc, sizeof(calc), "%s+%s+%s", field1, field2, field3);

    /* initialize OUT PV */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, 0.0);
    
    /* initialize input PVs */
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);
    testdbPutFieldOk("test_ao3.VAL", DBF_DOUBLE, val3);

    /* assign input PVs to calcout INP field */
    testdbPutFieldOk(inp1, DBF_STRING, "test_ao1");
    testdbPutFieldOk(inp2, DBF_STRING, "test_ao2");
    testdbPutFieldOk(inp3, DBF_STRING, "test_ao3");
    
    /* set calcout record's CALC and OUT field */
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, calc);
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out");
    
    /* process calout record */
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    
    /* check if OUT PV was updated to correct value */
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);
    
    /* number of tests = 11 * 21 /3  = 77 */
}   

static void test_oopt_on_change(void){
    double val1 = 1.0;
    double val2 = 2.0;
    double sum = val1 + val2;

    double val3 = 9.0;

    /* initialize OUT PV */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, 0.0);

    /* initialize input PVs */
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    /* set INPA, INPB, CALC, and OUT fields */
    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");

    /* change OOPT to 'On Change' */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_On_Change);

    /* process calcout record */
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* check if OUT link updated */
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);

    /********************************************************
     *  put new value to OUT PV different from current value
     *  process the record
     *  then make sure the OUT PV value did not change
     ********************************************************/
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);   
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val3);

    /* reset OOPT to default (Every Time) */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    /* number of tests = 14 */
}

static void test_oopt_every_time(void){
    double val1 = 3.0;
    double val2 = 4.0;
    double sum = val1 + val2;

    double val3 = 0.5;

    /* initialize OUT PV */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, 0.0);

    /* initialize input PVs */
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    /* set INPA, INPB, CALC, and OUT fields */
    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out");

    /* change OOPT to 'Every Time' */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    /* process calcout record */
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* check if OUT link updated */
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);

    /********************************************************
     *  put new value to OUT PV different from current value
     *  process the record
     *  then make sure the OUT PV value changed
     ********************************************************/
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, val3);
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, sum);

    /* number of tests = 13 */
}

static void test_oopt_when_zero(void){
    double val1 = 3.0;
    double val2 = -2.0;
    double zval = 0.0;
    double nz_val = -12.3;

    /* initialize OUT PV to non-zero value */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, nz_val);

    /* initialize input PVs */
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    /* set INPA, INPB, CALC, and OUT fields */
    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B-1");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out");

    /* change OOPT to 'When Zero' */ 
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_When_Zero);

    /* process calcout record */
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

     /* check if OUT link updated */
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, zval);

    /********************************************************
     *  put new value to OUT PV different from current value
     *  update CALC field so result is not equal to zero
     *  process calcout record
     *  then make sure the OUT PV value did not change
     ********************************************************/
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, nz_val);
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, val1);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, nz_val);

    /* reset OOPT to default (Every Time) */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    /* number of tests = 16 */
}

static void test_oopt_when_non_zero(void){
    double val1 = 1.0;
    double val2 = -1.0;
    double zval = 0.0;
    double nz_val = 22;

    /* initialize OUT and INP PVs */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, zval);
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    /* set INPA, INPB, CALC, and OUT fields */
    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* change OOPT to 'When Non Zero' and process record */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_When_Non_zero);
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* check if OUT link updated */
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val1);

    /********************************************************
     *  put new value to OUT PV different from current value
     *  update CALC field so result is not equal to zero
     *  process calcout record
     *  then make sure the OUT PV value did not change
     ********************************************************/
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, nz_val);
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, zval);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, nz_val);

    /* reset OOPT to default (Every Time) */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    /* number of tests = 16 */
}

static void test_oopt_transition_to_zero(void){
    double val1 = 3.0;
    double val2 = 4.0;
    double zval = 0.0;
    double nz_val = 123;

    /* initialize OUT and INP PVs */
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, zval);
    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, zval);

    /* set INPA, INPB, CALC, and OUT fields */
    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out");

    /* change OOPT to 'When Non Zero' and process record for a non-zero VAL */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Transition_To_Zero);
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* update CALC to get another non-zero VAL */
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "B-A");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);

    /* check if OUT link updated */
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, val1+val2);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, zval);

    /********************************************************
     *  put new value to OUT PV different from current value
     *  update CALC field so result is equal to zero
     *  process calcout record
     *  then make sure the OUT PV value did not change
     ********************************************************/
    testdbPutFieldOk("test_out.VAL", DBF_DOUBLE, nz_val);
    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A-B+1");
    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, zval);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, zval);

    /* reset OOPT to default (Every Time) */
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    /* number of tests = 17 */
}


static void test_oopt_transition_to_non_zero(void){
    double val1 = 1.0;
    double val2 = 2.0;
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

    // reset OOPT to default (Every Time)
    testdbPutFieldOk("test_calcout.OOPT", DBF_ENUM, calcoutOOPT_Every_Time);

    // number of tests = 15
}

static void test_dopt_use_calc(void){
    double val1 = 3.0;
    double val2 = 2.0;
    double val3 = val1 + val2;
    double val4 = val1 - val2;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.DOPT", DBF_ENUM, calcoutDOPT_Use_VAL);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OCAL", DBF_STRING, "A-B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");

   testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.DOPT", DBF_STRING, "Use CALC");
    testdbGetFieldEqual("test_calcout.VAL", DBF_DOUBLE, val3);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val3);

    // number of tests = 13
}

static void test_dopt_use_ocal(void){
    double val1 = 33.0;
    double val2 = 22.0;
    double val3 = val1 + val2;
    double val4 = val1 - val2;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);
    testdbPutFieldOk("test_ao2.VAL", DBF_DOUBLE, val2);

    testdbPutFieldOk("test_calcout.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout.INPB", DBF_STRING, "test_ao2");
    testdbPutFieldOk("test_calcout.DOPT", DBF_ENUM, calcoutDOPT_Use_OVAL);

    testdbPutFieldOk("test_calcout.CALC", DBF_STRING, "A+B");
    testdbPutFieldOk("test_calcout.OCAL", DBF_STRING, "A-B");
    testdbPutFieldOk("test_calcout.OUT", DBF_STRING, "test_out PP");

    testdbPutFieldOk("test_calcout.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("test_calcout.DOPT", DBF_STRING, "Use OCAL");
    testdbGetFieldEqual("test_calcout.OVAL", DBF_DOUBLE, val4);
    testdbGetFieldEqual("test_out.VAL", DBF_DOUBLE, val4);

    /* reset dopt to default (Use VAL / Use CALC) */
    testdbPutFieldOk("test_calcout.DOPT", DBF_ENUM, calcoutDOPT_Use_VAL);

    /* number of tests = 11 */
}


static void test_input_link_status(void){
    double val1 = 123.0;
    double val2 = 345.1;

    testdbPutFieldOk("test_ao1.VAL", DBF_DOUBLE, val1);

    testdbPutFieldOk("test_calcout2.INPA", DBF_STRING, "test_ao1");
    testdbPutFieldOk("test_calcout2.INPB", DBF_STRING, "345.1");
    testdbPutFieldOk("test_calcout2.C", DBF_STRING, "345.1");
    //testdbPutFieldOk("test_calcout2.D", DBF_STRING, "{789.1}");
    //testdbPutFieldOk("test_calcout2.INPE", DBF_STRING, "{789.1}");

    // check link status
    testdbGetFieldEqual("test_calcout2.INAV", DBF_STRING, "Local PV");
    testdbGetFieldEqual("test_calcout2.INBV", DBF_STRING, "Constant");
    testdbGetFieldEqual("test_calcout2.INCV", DBF_STRING, "Constant");
    testdbGetFieldEqual("test_calcout2.INDV", DBF_STRING, "Constant");
}

MAIN(calcoutTest){
    //testPlan(8+12+12+11+11+14+14+15+12);
    testPlan(206);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("calcoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    /* test_sum for all INP fields in groups of 3 */
    for (i = 0; i < NUM_OF_INPUTS; i+=3) {
        test_sum_grp(t_INPField[i], t_INPField[i+1], t_INPField[i+2], i);
    }

    test_oopt_on_change();
    test_oopt_every_time();
    test_oopt_when_zero();
    test_oopt_when_non_zero();
    test_oopt_transition_to_zero();
    test_oopt_transition_to_non_zero();

    test_dopt_use_calc();
    test_dopt_use_ocal();

    testTodoBegin("Known BUG per Andrew");
    test_input_link_status();
    testTodoEnd();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
