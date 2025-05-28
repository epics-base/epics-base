/*************************************************************************\
* Copyright (c) 2023 Karl Vestin
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuIvoa.h"
#include "epicsThread.h"
#include <stdlib.h>

#include "dfanoutRecord.h"

#define SLEEP_TIME 0.01
#define SLEEP epicsThreadSleep(SLEEP_TIME);

char dfanout_OUT_pvs[8][25] = {{"test_dfanout_record.OUTA\0"}, {"test_dfanout_record.OUTB\0"},
                           {"test_dfanout_record.OUTC\0"}, {"test_dfanout_record.OUTD\0"},
                           {"test_dfanout_record.OUTE\0"}, {"test_dfanout_record.OUTF\0"},
                           {"test_dfanout_record.OUTG\0"}, {"test_dfanout_record.OUTH\0"}};

char dfanout_receivers[8][18] = {{"test_dfanout_outa\0"}, {"test_dfanout_outb\0"},
                                 {"test_dfanout_outc\0"}, {"test_dfanout_outd\0"},
                                 {"test_dfanout_oute\0"}, {"test_dfanout_outf\0"},
                                 {"test_dfanout_outg\0"}, {"test_dfanout_outh\0"}};

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_all(int val, int exception){

    SLEEP // Needed, unfortunately :(

    // if i < 0 or > 8 then it tests all.
    for (uint i = 0; i < 8; ++i) {
        if ( i == exception) continue;
        testdbGetFieldEqual(dfanout_receivers[i], DBF_LONG, val);
    }

}

static void test_all_output(void){
    
    /* set output fields */
    for (uint i = 0; i < 8; ++i) {
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

    for (int val = 0; val < 8; ++val) {
        testdbPutFieldOk("test_dfanout_record.SELN", DBF_LONG, val + 1);
        testdbPutFieldOk("test_dfanout_src.VAL", DBF_LONG, val + 1);
        SLEEP

        testdbGetFieldEqual(dfanout_receivers[val], DBF_LONG, val + 1);
        
        int not_seln_val;
        for (int not_seln = 0; not_seln < 8; ++not_seln) {
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

MAIN(dfanoutTest) {

    testPlan(117);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dfanoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_all_output();
    test_selm_specified();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
