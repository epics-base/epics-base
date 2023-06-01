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
#include "menuConvert.h"
#include "epicsMath.h"
#include "menuScan.h"
#include "caeventmask.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_soft_input(void){
    double value = 543.123;

    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set VAL to on linked record */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, value);

    /* verify that this record VAL is updated but RVAL is not */
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_DOUBLE, value);
    testdbGetFieldEqual("test_ai_rec.RVAL", DBF_DOUBLE, 0.0);

    // number of tests = 6
}

static void test_raw_soft_input(void){
    double value = -255;

    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set VAL to on linked record */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, value);

    /* verify that this record RVAL and VAL are updated */
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_DOUBLE, value);
    testdbGetFieldEqual("test_ai_rec.RVAL", DBF_DOUBLE, value);

    // number of tests = 6
}

static void test_operator_display(void){
    const short hopr = 199;
    const short lopr = 50;
    const short prec = 2;
    const char egu[] = "mm";
    const char desc[] = "hmm?";

    /* set operator display parameters */
    testdbPutFieldOk("test_ai_rec.EGU", DBF_STRING, egu);
    testdbPutFieldOk("test_ai_rec.DESC", DBF_STRING, desc);
    testdbPutFieldOk("test_ai_rec.HOPR", DBF_SHORT, hopr);
    testdbPutFieldOk("test_ai_rec.LOPR", DBF_SHORT, lopr);
    testdbPutFieldOk("test_ai_rec.PREC", DBF_SHORT, prec);
    
    /* verify operator display parameters read back */
    testdbGetFieldEqual("test_ai_rec.NAME", DBF_STRING, "test_ai_rec");
    testdbGetFieldEqual("test_ai_rec.EGU", DBF_STRING, egu);   
    testdbGetFieldEqual("test_ai_rec.DESC", DBF_STRING, desc);
    testdbGetFieldEqual("test_ai_rec.HOPR", DBF_SHORT, hopr);
    testdbGetFieldEqual("test_ai_rec.LOPR", DBF_SHORT, lopr);
    testdbGetFieldEqual("test_ai_rec.PREC", DBF_SHORT, prec); 

    // number of tests = 7
}

static void test_no_linr_unit_conversion(void){
    const short roff = 10;
    const short aslo = 2;
    const short aoff = 4;

    const short rval = 9;
    const short val = (((rval + roff) * aslo) + aoff);


    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set unit conversion parameters */
    testdbPutFieldOk("test_ai_rec.ROFF", DBF_SHORT, roff);
    testdbPutFieldOk("test_ai_rec.ASLO", DBF_SHORT, aslo);
    testdbPutFieldOk("test_ai_rec.AOFF", DBF_SHORT, aoff);
    testdbPutFieldOk("test_ai_rec.LINR", DBF_LONG, menuConvertNO_CONVERSION);
    
    /* verify conversion */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_SHORT, rval);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_SHORT, val);
    testdbGetFieldEqual("test_ai_rec.RVAL", DBF_SHORT, rval);

    // number of tests = 10
}

static void test_slope_linr_unit_conversion(void){
    const short roff = 1;
    const short aslo = 3;
    const short aoff = 99;
    const short eslo = 3;
    const short eoff = 2;

    const short rval = 7;
    const short val = ((((rval + roff) * aslo) + aoff) * eslo) + eoff;


    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set unit conversion parameters */
    testdbPutFieldOk("test_ai_rec.ROFF", DBF_LONG, roff);
    testdbPutFieldOk("test_ai_rec.ASLO", DBF_LONG, aslo);
    testdbPutFieldOk("test_ai_rec.AOFF", DBF_LONG, aoff);
    testdbPutFieldOk("test_ai_rec.ESLO", DBF_LONG, eslo);
    testdbPutFieldOk("test_ai_rec.EOFF", DBF_LONG, eoff);
    testdbPutFieldOk("test_ai_rec.LINR", DBF_LONG, menuConvertSLOPE);
    
    /* verify conversion */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_LONG, rval);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_LONG, val);
    testdbGetFieldEqual("test_ai_rec.RVAL", DBF_LONG, rval);

    // number of tests = 12
}

static void test_linear_linr_unit_conversion(void){
    const long roff = 6;
    const long aslo = 2;
    const long aoff = 19;
    const long eslo = 4;
    const long eoff = 1;
    const long egul = 9999;
    const long eguf = -1000;

    const long rval = 2;
    /* Since our raw soft input does not support EGUL and EGUF conversion LINEAR should work like SLOPE */
    const long val = ((((rval + roff) * aslo) + aoff) * eslo) + eoff;


    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set unit conversion parameters */
    testdbPutFieldOk("test_ai_rec.ROFF", DBF_LONG, roff);
    testdbPutFieldOk("test_ai_rec.ASLO", DBF_LONG, aslo);
    testdbPutFieldOk("test_ai_rec.AOFF", DBF_LONG, aoff);
    testdbPutFieldOk("test_ai_rec.ESLO", DBF_LONG, eslo);
    testdbPutFieldOk("test_ai_rec.EOFF", DBF_LONG, eoff);
    /* Since our raw soft input does not support EGUL and EGUF conversion we set them to lagre values here just to check they dont break anything  */
    testdbPutFieldOk("test_ai_rec.EGUL", DBF_LONG, egul);
    testdbPutFieldOk("test_ai_rec.EGUF", DBF_LONG, eguf);
    testdbPutFieldOk("test_ai_rec.LINR", DBF_LONG, menuConvertLINEAR);
    
    /* verify conversion */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_LONG, rval);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_LONG, val);
    testdbGetFieldEqual("test_ai_rec.RVAL", DBF_LONG, rval);

    // number of tests = 14
}

static void test_bpt_conversion(void){
    const float typeKdegFin = 699.550510;
    const float typeKdegF = 3.427551e+02;
    const float typeKdegCin = 2902.787322;
    const float typeKdegC = 7.028131e+02;
    const float typeJdegFin = 1218.865107;
    const float typeJdegF = 3.897504e+02;
    const float typeJdegCin = 4042.988372;
    const float typeJdegC = 6.918437e+02;

    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set unit conversion parameters */
    testdbPutFieldOk("test_ai_rec.ROFF", DBF_LONG, 0);
    testdbPutFieldOk("test_ai_rec.ASLO", DBF_LONG, 1);
    testdbPutFieldOk("test_ai_rec.AOFF", DBF_LONG, 0);

    /* Set type K deg F break point table and verify */
    testdbPutFieldOk("test_ai_rec.LINR", DBF_SHORT, menuConverttypeKdegF);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_FLOAT, typeKdegFin);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_FLOAT, typeKdegF);

    /* Set type K deg C break point table and verify */
    testdbPutFieldOk("test_ai_rec.LINR", DBF_SHORT, menuConverttypeKdegC);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_FLOAT, typeKdegCin);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_FLOAT, typeKdegC);

    /* Set type J deg F break point table and verify */
    testdbPutFieldOk("test_ai_rec.LINR", DBF_SHORT, menuConverttypeJdegF);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_FLOAT, typeJdegFin);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_FLOAT, typeJdegF);

    /* Set type J deg C break point table and verify */
    testdbPutFieldOk("test_ai_rec.LINR", DBF_SHORT, menuConverttypeJdegC);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_FLOAT, typeJdegCin);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_FLOAT, typeJdegC);

    // number of tests = 18
}

static void test_smoothing_filter(void){
    const double smoo = 0.7;
    const short roff = 1;
    const short aslo = 3;
    const short aoff = -2;
    const short eslo = 2;
    const short eoff = 6;

    const short rval1 = 7;
    const short val1 = ((((rval1 + roff) * aslo) + aoff) * eslo) + eoff;
    const short rval2 = 9;
    const short val2_pre_smoo = ((((rval2 + roff) * aslo) + aoff) * eslo) + eoff;
    const float val2 = val1 * smoo + (1 - smoo) * val2_pre_smoo;


    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set unit conversion parameters */
    testdbPutFieldOk("test_ai_rec.ROFF", DBF_LONG, roff);
    testdbPutFieldOk("test_ai_rec.ASLO", DBF_LONG, aslo);
    testdbPutFieldOk("test_ai_rec.AOFF", DBF_LONG, aoff);
    testdbPutFieldOk("test_ai_rec.ESLO", DBF_LONG, eslo);
    testdbPutFieldOk("test_ai_rec.EOFF", DBF_LONG, eoff);
    testdbPutFieldOk("test_ai_rec.LINR", DBF_LONG, menuConvertSLOPE);

    /* set well known starting point (without smmothing) */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_SHORT, rval1);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_SHORT, val1);
    
    /* set SMOO */
    testdbPutFieldOk("test_ai_rec.SMOO", DBF_DOUBLE, smoo);

    /* update value again and verify smoothing */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_SHORT, rval2);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_FLOAT, val2);

    /* clear SMOO */
    testdbPutFieldOk("test_ai_rec.SMOO", DBF_DOUBLE, 0.0);

    // number of tests = 15
}

static void test_udf(void){
    /* set soft channel */
    testdbPutFieldOk("test_ai_rec2.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec2.INP", DBF_STRING, "test_ai_link_rec2.VAL");
    testdbPutFieldOk("test_ai_link_rec2.FLNK", DBF_STRING, "test_ai_rec2");

    /* verify UDF */
    testdbGetFieldEqual("test_ai_rec2.UDF", DBF_CHAR, TRUE);

    /* set VAL and verify UDF is cleared */
    testdbPutFieldOk("test_ai_link_rec2.VAL", DBF_FLOAT, 7.0);
    testdbGetFieldEqual("test_ai_rec2.UDF", DBF_CHAR, FALSE);

    // number of tests = 6
}

static void test_alarm(void){
    const double h = 5000;
    const double hh = 7500;
    const double l = 200;
    const double ll = -20;
    const double hyst = 5;

    /* set soft channel */
    testdbPutFieldOk("test_ai_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_ai_rec.INP", DBF_STRING, "test_ai_link_rec.VAL");
    testdbPutFieldOk("test_ai_link_rec.FLNK", DBF_STRING, "test_ai_rec");

    /* set alarm parameters */
    testdbPutFieldOk("test_ai_rec.HIGH", DBF_DOUBLE, h);
    testdbPutFieldOk("test_ai_rec.HIHI", DBF_DOUBLE, hh);
    testdbPutFieldOk("test_ai_rec.LOW", DBF_DOUBLE, l);
    testdbPutFieldOk("test_ai_rec.LOLO", DBF_DOUBLE, ll);
    testdbPutFieldOk("test_ai_rec.HHSV", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbPutFieldOk("test_ai_rec.HSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_ai_rec.LSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_ai_rec.LLSV", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set non alarm VAL and verify */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, 400.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrNO_ALARM);

    /* set LOW alarm VAL and verify */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, 190.0);
    testdbGetFieldEqual("test_ai_rec.VAL", DBF_DOUBLE, 190.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMINOR);

    /* set LOLO alarm VAL and verify */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, -9998.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set HIGH alarm VAL and verify */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, 6111.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMINOR);

    /* set HIHI alarm VAL and verify */
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, 19998.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set HYST, verify and clear */
    testdbPutFieldOk("test_ai_rec.HYST", DBF_DOUBLE, hyst);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, hh - 1.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbPutFieldOk("test_ai_rec.HYST", DBF_DOUBLE, 0.0);
    testdbPutFieldOk("test_ai_link_rec.VAL", DBF_DOUBLE, hh - 2.0);
    testdbGetFieldEqual("test_ai_rec.SEVR", DBF_SHORT, menuAlarmSevrMINOR);

    /* verify LALM */
    testdbGetFieldEqual("test_ai_rec.LALM", DBF_DOUBLE, h);

    // number of tests = 29
}

static void test_aftc(void){
    const double h = 5000;
    const double hh = 7500;
    const double l = 200;
    const double ll = -20;
    const double aftc = 3;
    testMonitor* test_mon = NULL;
    epicsTimeStamp startTime;
    epicsTimeStamp endTime;
    double diffTime = 0;

    /* set soft channel */
    testdbPutFieldOk("test_ai_rec3.DTYP", DBF_STRING, "Raw Soft Channel");
    testdbPutFieldOk("test_ai_rec3.INP", DBF_STRING, "test_ai_link_rec3.VAL");
    testdbPutFieldOk("test_ai_link_rec3.FLNK", DBF_STRING, "test_ai_rec3");

    /* set alarm parameters */
    testdbPutFieldOk("test_ai_rec3.HIGH", DBF_DOUBLE, h);
    testdbPutFieldOk("test_ai_rec3.HIHI", DBF_DOUBLE, hh);
    testdbPutFieldOk("test_ai_rec3.LOW", DBF_DOUBLE, l);
    testdbPutFieldOk("test_ai_rec3.LOLO", DBF_DOUBLE, ll);
    testdbPutFieldOk("test_ai_rec3.HHSV", DBF_SHORT, menuAlarmSevrMAJOR);
    testdbPutFieldOk("test_ai_rec3.HSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_ai_rec3.LSV", DBF_SHORT, menuAlarmSevrMINOR);
    testdbPutFieldOk("test_ai_rec3.LLSV", DBF_SHORT, menuAlarmSevrMAJOR);

    /* test AFTC using a monitor and time stamps */
    testdbPutFieldOk("test_ai_rec3.AFTC", DBF_DOUBLE, aftc);
    testdbPutFieldOk("test_ai_rec3.SCAN", DBF_SHORT, menuScan_1_second);
    
    /* set HIHI alarm VAL */
    testdbPutFieldOk("test_ai_link_rec3.VAL", DBF_DOUBLE, 7550.0);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_ai_rec3.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status*/
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now MAJOR */
    testdbGetFieldEqual("test_ai_rec3.SEVR", DBF_SHORT, menuAlarmSevrMAJOR);

    /* set HI alarm VAL */
    testdbPutFieldOk("test_ai_link_rec3.VAL", DBF_DOUBLE, 5550.0);

    /* Create test monitor for alarm SEVR */
    test_mon = testMonitorCreate("test_ai_rec3.VAL", DBE_ALARM, 0);

    /* Get start time */
    epicsTimeGetCurrent(&startTime);

    /* wait for monitor to trigger on the new alarm status*/
    testMonitorWait(test_mon);
    epicsTimeGetCurrent(&endTime);

    /* Verify that alarm status is now MINOR */
    testdbGetFieldEqual("test_ai_rec3.SEVR", DBF_SHORT, menuAlarmSevrMINOR);
    
    /* Verify that time is at least equal to configured aftc */
    diffTime = epicsTimeDiffInSeconds(&endTime, &startTime);
    testOk(diffTime >= aftc, "ATFC time %lf", diffTime);
 
    // number of tests = 18
}

MAIN(aiTest) {

#ifdef _WIN32
#if (defined(_MSC_VER) && _MSC_VER < 1900) || \
    (defined(_MINGW) && defined(_TWO_DIGIT_EXPONENT))
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
#endif

    testPlan(6+6+11+10+12+14+18+15+6+29+18);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("aiTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_soft_input();
    test_raw_soft_input();
    test_operator_display();
    test_no_linr_unit_conversion();
    test_slope_linr_unit_conversion();
    test_linear_linr_unit_conversion();
    test_bpt_conversion();
    test_smoothing_filter();
    test_udf();
    test_alarm();
    test_aftc(); 

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
