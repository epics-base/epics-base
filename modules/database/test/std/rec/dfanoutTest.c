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
#include "dfanoutRecord.h"

static const char *dfanout_OUT_pvs[] = {"test_dfanout_record.OUTA", "test_dfanout_record.OUTB",
    "test_dfanout_record.OUTC", "test_dfanout_record.OUTD",
    "test_dfanout_record.OUTE", "test_dfanout_record.OUTF",
    "test_dfanout_record.OUTG", "test_dfanout_record.OUTH"};

static const char *dfanout_receivers[] = {"test_dfanout_outa", "test_dfanout_outb",
    "test_dfanout_outc", "test_dfanout_outd",
    "test_dfanout_oute", "test_dfanout_outf",
    "test_dfanout_outg", "test_dfanout_outh"};

static testMonitor *monitor;

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_all(int val, int exception){

    testMonitorWait(monitor);
    // if exception < 0 or > 8 then it tests all.
    int i;
    for (i = 0; i < NELEMENTS(dfanout_receivers); ++i) {
        if ( i == exception) continue;
        testdbGetFieldEqual(dfanout_receivers[i], DBF_LONG, val);
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
        testMonitorWait(monitor);

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

    /* Resets values. Tests if fields in bitmask have been set */
    int mask;
    for (mask = 0; mask <= 255; ++mask) {

        testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "All"); //Setting all values to 1 so we know what to compare with.
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 1);
        testMonitorWait(monitor);

        testdbPutFieldOk("test_dfanout_record.SELM", DBF_STRING, "Mask");
        testdbPutFieldOk("test_dfanout_record.SELN", DBF_LONG, mask);
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, 9);
        testMonitorWait(monitor);

        int item;
        for (item = 0; item < NELEMENTS(dfanout_receivers); ++item) {

            if ( mask & (1 << item) ) { // If i represents a set bit in the bitmask
                testdbGetFieldEqual(dfanout_receivers[item], DBF_LONG, 9);
            } else {
                testdbGetFieldEqual(dfanout_receivers[item], DBF_LONG, 1);
            }

        }

    }

}

MAIN(dfanoutTest) {

    testPlan(3455);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dfanoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    monitor = testMonitorCreate("test_dfanout_record", DBE_VALUE, 0);
    test_all_output();
    test_selm_specified();
    test_selm_mask();
    testMonitorDestroy(monitor);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
