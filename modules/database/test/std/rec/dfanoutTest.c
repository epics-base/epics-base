/*************************************************************************\
* Copyright (c) 2023 Marco Montevechi Filho
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuIvoa.h"
#include "epicsThread.h"
#include "epicsMath.h"
#include "dfanoutRecord.h"

static const char *dfanout_OUT_pvs[] = {
    "test_dfanout_record.OUTA", "test_dfanout_record.OUTB",
    "test_dfanout_record.OUTC", "test_dfanout_record.OUTD",
    "test_dfanout_record.OUTE", "test_dfanout_record.OUTF",
    "test_dfanout_record.OUTG", "test_dfanout_record.OUTH",
    "test_dfanout_record.OUTI", "test_dfanout_record.OUTJ",
    "test_dfanout_record.OUTK", "test_dfanout_record.OUTL",
    "test_dfanout_record.OUTM", "test_dfanout_record.OUTN",
    "test_dfanout_record.OUTO", "test_dfanout_record.OUTP"};

static const char *dfanout_receivers[] = {
    "test_dfanout_outa", "test_dfanout_outb",
    "test_dfanout_outc", "test_dfanout_outd",
    "test_dfanout_oute", "test_dfanout_outf",
    "test_dfanout_outg", "test_dfanout_outh",
    "test_dfanout_outi", "test_dfanout_outj",
    "test_dfanout_outk", "test_dfanout_outl",
    "test_dfanout_outm", "test_dfanout_outn",
    "test_dfanout_outo", "test_dfanout_outp"};

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_all(double val, int exception){

    // if mask == -1 then it tests all.
    int i;
    for (i = 0; i < NELEMENTS(dfanout_receivers); ++i) {
        if ( i == exception) continue;
        testdbGetFieldEqual(dfanout_receivers[i], DBF_DOUBLE, val);
    }
}

static void test_all_output(void){

    /* set output fields */
    int i;
    for (i = 0; i < NELEMENTS(dfanout_OUT_pvs); ++i) {
        testdbPutFieldOk(dfanout_OUT_pvs[i], DBF_STRING, dfanout_receivers[i]);
    }

    /* set VAL from src to any random number */
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 5);

    /* verify that OUT records are updated */
    test_all(5, -1);

}

static void test_selm_specified() {

    /* Resetting values */
    testDiag("Testing Specified");
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 0);
    test_all(0, -1);

    testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "Specified");
    testdbPutFieldOk("test_dfanout_record.SELN", DBF_LONG, 0);
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 10);
    test_all(0, -1);

    int val;
    for (val = 0; val < NELEMENTS(dfanout_receivers); ++val) {
        testdbPutFieldOk("test_dfanout_record.SELN", DBF_LONG, val + 1);
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, val + 1);

        testdbGetFieldEqual(dfanout_receivers[val], DBF_LONG, val + 1);

        int not_seln, not_seln_val;
        for (not_seln = 0; not_seln < NELEMENTS(dfanout_receivers); ++not_seln) {
            if (not_seln == val) continue;
            if (not_seln < val) { // If record has already been tested, expected value is index + 1
                not_seln_val = not_seln + 1;
            }  else { // If record hasn't been tested yet, expected value is 0
                not_seln_val = 0;
            }
            testdbGetFieldEqual(dfanout_receivers[not_seln], DBF_LONG, not_seln_val);
        }

    }

}

static void test_selm_mask() {

    /* Resetting values */
    testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "All");
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 0);
    test_all(0, -1);

    /* Testing all 65535 possible masks seems a bit excessive -- just test some patterns */
    epicsUInt32 mask = 0x100055aa;
    while (1) {
        testDiag("Testing mask 0x%04x", mask);
        /* Resets values. Tests if fields in bitmask have been set */
        testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "All"); //Setting all values to 1 so we know what to compare with.
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 1);

        testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "Mask");
        testdbPutFieldOk("test_dfanout_record.SELN", DBF_LONG, mask);
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 9);

        int item;
        for (item = 0; item < NELEMENTS(dfanout_receivers); ++item) {

            if ( mask & (1 << item) ) { // If i represents a set bit in the bitmask
                testdbGetFieldEqual(dfanout_receivers[item], DBF_LONG, 9);
            } else {
                testdbGetFieldEqual(dfanout_receivers[item], DBF_LONG, 1);
            }

        }
        if (!mask) break;
        mask >>= 1;
    }
}

static void test_ivoa() {


    testDiag("Testing IVOA = Continue normally");
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 1);
    testdbPutFieldOk("test_dfanout_record.IVOA", DBF_STRING, "Continue normally");
    testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "All");
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_DOUBLE, epicsNAN);
    test_all(epicsNAN, -1);
    testdbGetFieldEqual("test_dfanout_record.VAL", DBF_DOUBLE, epicsNAN);

    testDiag("Testing IVOA = Don't drive outputs");
    testdbPutFieldOk("test_dfanout_record.IVOA", DBF_STRING, "Don't drive outputs");
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_DOUBLE, 1.2345);
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_DOUBLE, epicsNAN);
    test_all(1.2345, -1);
    testdbGetFieldEqual("test_dfanout_record.VAL", DBF_DOUBLE, epicsNAN);

    testDiag("Testing IVOA = Set output to IVOV");
    testdbPutFieldOk("test_dfanout_record.IVOA", DBF_STRING, "Set output to IVOV");
    testdbPutFieldOk("test_dfanout_record.IVOV", DBF_DOUBLE, 3.1415);
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_DOUBLE, 42);
    testdbPutFieldOk("test_dfanout_src.VAL", DBF_DOUBLE, epicsNAN);
    test_all(3.1415, -1);
    testdbGetFieldEqual("test_dfanout_record.VAL", DBF_DOUBLE, 3.1415);
}

MAIN(dfanoutTest) {

    testPlan(1067);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dfanoutTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    test_all_output();
    test_selm_specified();
    test_selm_mask();
    test_ivoa();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
