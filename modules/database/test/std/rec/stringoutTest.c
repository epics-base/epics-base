/*************************************************************************\
* Copyright (c) 2025 Grzegorz Kowalski
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbFldTypes.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#include "stringoutRecord.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_soft_output(void) {
    /* set soft channel */
    testdbPutFieldOk("test_stringout_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_rec.OUT", DBF_STRING, "test_link_rec");

    const char* test_str = "test string";

    /* set VAL to process record */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str);

    /* verify that OUT record is updated */
    testdbGetFieldEqual("test_link_rec.VAL", DBF_STRING, test_str);

    // number of tests = 4
}

static void test_truncation(void) {
    /* set soft channel */
    testdbPutFieldOk("test_stringout_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_rec.OUT", DBF_STRING, "test_link_rec");

    const char* test_str = "01234567890123456789012345678901234567890123456789";
    // the documentation is wrong, the string is truncated to 39 chars
    const char* trunc_39 = "012345678901234567890123456789012345678";

    /* set VAL to a string longer than stringout record limit */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str);

    /* verify that OUT record is truncated to 39 chars */
    testdbGetFieldEqual("test_link_rec.VAL", DBF_STRING, trunc_39);

    // number of tests = 4
}

static void test_desired_output(void) {
    /* set soft channel */
    testdbPutFieldOk("test_stringout_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_rec.OUT", DBF_STRING, "test_link_rec");

    /* set input link and output mode select */
    testdbPutFieldOk("test_stringout_rec.DOL", DBF_STRING, "test_dol_rec.VAL");
    testdbPutFieldOk("test_stringout_rec.OMSL", DBF_SHORT, menuOmslclosed_loop);

    const char* test_str = "string from link";

    /* set DOL record value */
    testdbPutFieldOk("test_dol_rec.VAL", DBF_STRING, test_str);

    /* process the test record */
    testdbPutFieldOk("test_stringout_rec.PROC", DBF_SHORT, 1);

    /* verify that OUT record value comes from DOL link */
    testdbGetFieldEqual("test_stringout_rec.VAL", DBF_STRING, test_str);

    /* clean up after test */
    testdbPutFieldOk("test_stringout_rec.OMSL", DBF_SHORT, menuOmslsupervisory);

    // number of tests = 8
}

static void test_monitor_params(void) {
    /* set soft channel */
    testdbPutFieldOk("test_stringout_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_rec.OUT", DBF_STRING, "test_link_rec");

    /* create monitor */
    testMonitor* test_mon = testMonitorCreate("test_stringout_rec.VAL", DBR_SHORT, 0);

    const char* test_str1 = "test string";
    const char* test_str2 = "another test string";

    /* set up initial value */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str1);

    /* always post monitors */
    testdbPutFieldOk("test_stringout_rec.MPST", DBF_SHORT, stringoutPOST_Always);

    /* put the same value again */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str1);

    /* verify that monitor post was received */
    testMonitorWait(test_mon);

    /* post monitors only on value change */
    testdbPutFieldOk("test_stringout_rec.MPST", DBF_SHORT, stringoutPOST_OnChange);

    /* reset the monitor counter */
    testMonitorCount(test_mon, 1);

    /* put the same value again */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str1);

    /* check that no monitors have been received */
    epicsThreadSleep(1.0); // TODO: monitor subsystem is missing a utility for
                           // checking for the absence of a monitor event
    unsigned count = testMonitorCount(test_mon, 0);
    testOk(count == 0, "Monitor count after same value put: %u", count);

    /* put different value again */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str2);

    /* verify that monitor post was received */
    testMonitorWait(test_mon);

    testMonitorDestroy(test_mon);

    // number of tests = 9
}

static void test_simulation_mode(void) {
    /* set soft channel */
    testdbPutFieldOk("test_stringout_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_rec.OUT", DBF_STRING, "test_link_rec");

    const char* test_str = "test string";

    /* set a value so that test_link_rec is updated */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_str);

    /* verify that linked record has updated */
    testdbGetFieldEqual("test_link_rec.VAL", DBF_STRING, test_str);

    /* set up simulation mode */
    testdbPutFieldOk("test_stringout_rec.SIOL", DBF_STRING, "test_sim_rec");
    testdbPutFieldOk("test_stringout_rec.SIMM", DBF_SHORT, menuYesNoYES);

    const char* test_sim_str = "test string simulation";

    /* write new value */
    testdbPutFieldOk("test_stringout_rec.VAL", DBF_STRING, test_sim_str);

    /* verify that simulation link got the value */
    testdbGetFieldEqual("test_sim_rec.VAL", DBF_STRING, test_sim_str);

    /* verify that non-simulation link still has the old value */
    testdbGetFieldEqual("test_link_rec.VAL", DBF_STRING, test_str);

    /* cleanup */
    testdbPutFieldOk("test_stringout_rec.SIMM", DBF_SHORT, menuYesNoNO);

    // number of tests = 10
}

static void test_invalid(void) {
    /* This test uses a fresh, not used record so that processing it sets the
     * severity to INVALID.
     */

    /* set soft channel */
    testdbPutFieldOk("test_stringout_invalid_rec.DTYP", DBF_STRING, "Soft Channel");
    testdbPutFieldOk("test_stringout_invalid_rec.OUT", DBF_STRING, "test_link_invalid_rec");

    const char* invalid_str = "invalid string";

    /* set up invalid action */
    testdbPutFieldOk("test_stringout_invalid_rec.IVOV", DBF_STRING, invalid_str);
    testdbPutFieldOk("test_stringout_invalid_rec.IVOA", DBF_SHORT, menuIvoaSet_output_to_IVOV);

    /* force the record to process */
    testdbPutFieldOk("test_stringout_invalid_rec.PROC", DBF_SHORT, 1);

    /* verify that the link gets the invalid action value */
    testdbGetFieldEqual("test_link_invalid_rec.VAL", DBF_STRING, invalid_str);
    
    // number of tests = 6
}

MAIN(stringoutTest) {

    testPlan(4+4+8+9+10+6);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("stringoutTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_soft_output();
    test_truncation();
    test_desired_output();
    test_monitor_params();
    test_simulation_mode();
    test_invalid();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
