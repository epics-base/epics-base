/*************************************************************************\
* Copyright (c) 2026 Nick Marks
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "epicsUnitTest.h"
#include "errlog.h"
#include "dbAccess.h"

#include "fanoutRecord.h"
#include "calcRecord.h"
#include "alarm.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static const char *fanout_record_LNK_fields[] = {
    "test_fanout_record.LNK0",
    "test_fanout_record.LNK1",
    "test_fanout_record.LNK2",
    "test_fanout_record.LNK3",
    "test_fanout_record.LNK4",
    "test_fanout_record.LNK5",
    "test_fanout_record.LNK6",
    "test_fanout_record.LNK7",
    "test_fanout_record.LNK8",
    "test_fanout_record.LNK9",
    "test_fanout_record.LNKA",
    "test_fanout_record.LNKB",
    "test_fanout_record.LNKC",
    "test_fanout_record.LNKD",
    "test_fanout_record.LNKE",
    "test_fanout_record.LNKF",
};

static const char *fanout_targets[] = {
    "test_fanout_out0",
    "test_fanout_out1",
    "test_fanout_out2",
    "test_fanout_out3",
    "test_fanout_out4",
    "test_fanout_out5",
    "test_fanout_out6",
    "test_fanout_out7",
    "test_fanout_out8",
    "test_fanout_out9",
    "test_fanout_outa",
    "test_fanout_outb",
    "test_fanout_outc",
    "test_fanout_outd",
    "test_fanout_oute",
    "test_fanout_outf",
};

#define NRECORDS NELEMENTS(fanout_targets)

static calcRecord *targets[NRECORDS];

static void init_target_records(void) {
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        targets[i] = (calcRecord *)testdbRecordPtr(fanout_targets[i]);
        if (!targets[i]) {
            testAbort("Record %s not found", fanout_targets[i]);
        }
    }
}

static void reset_targets(void) {
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        dbScanLock((dbCommon*)targets[i]);
        targets[i]->val = 0.0;
        dbScanUnlock((dbCommon*)targets[i]);
    }
}

enum {
    VALID = 1,
    INVALID = 0
};

typedef struct {
    const char *diag;
    fanoutSELM selm;
    epicsUInt16 seln;
    epicsInt16 shft;
    epicsInt16 offs;
    int state;
} TestCase;

static const TestCase selm_all = {
    "selm_all",
    fanoutSELM_All,
    0,
    0,
    0,
    VALID
};

static const TestCase selm_specified = {
    "selm_specified",
    fanoutSELM_Specified,
    4,
    0,
    2,
    VALID
};

static const TestCase selm_specified_lower_invalid = {
    "selm_specified_lower_invalid",
    fanoutSELM_Specified,
    2,
    0,
    -3,
    INVALID
};

static const TestCase selm_specified_upper_invalid = {
    "selm_specified_upper_invalid",
    fanoutSELM_Specified,
    15,
    0,
    1,
    INVALID
};

static const TestCase selm_mask = {
    "selm_mask",
    fanoutSELM_Mask,
    0x5555,
    0,
    0,
    VALID
};

static const TestCase selm_mask_shift_left = {
    "selm_mask_shift_left",
    fanoutSELM_Mask,
    0x0015,
    -2,
    0,
    VALID
};

static const TestCase selm_mask_shift_right = {
    "selm_mask_shift_right",
    fanoutSELM_Mask,
    0xa800,
    2,
    0,
    VALID
};

static const TestCase selm_mask_invalid_shift1 = {
    "selm_mask_invalid_shift(SHFT=16)",
    fanoutSELM_Mask,
    0xffff,
    16,
    0,
    INVALID
};

static const TestCase selm_mask_invalid_shift2 = {
    "selm_mask_invalid_shift(SHFT=-16)",
    fanoutSELM_Mask,
    0xffff,
    -16,
    0,
    INVALID
};

static const TestCase selm_mask_zero = {
    "selm_mask_zero",
    fanoutSELM_Mask,
    0,
    0,
    0,
    VALID
};

static void run_case(const TestCase *c) {
    testDiag("%s", c->diag);
    reset_targets();

    // Set the SELM, SELN, SHFT, and OFFS fields and process the fanout record
    testdbPutFieldOk("test_fanout_record.SELM", DBF_ENUM, c->selm);
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, c->seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, c->shft);
    testdbPutFieldOk("test_fanout_record.OFFS", DBF_SHORT, c->offs);
    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    if (c->state == INVALID) {
        // Invalid field values must not process any output link
        int i;
        for (i = 0; i < NRECORDS; ++i) {
            testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 0.0);
        }
        testdbGetFieldEqual("test_fanout_record.STAT", DBF_USHORT, SOFT_ALARM);
        testdbGetFieldEqual("test_fanout_record.SEVR", DBF_USHORT, INVALID_ALARM);
        return;
    }

    switch (c->selm) {
        int i;
        case fanoutSELM_All:
            // Check that all target records changed
            for (i = 0; i < NRECORDS; ++i) {
                testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 1.0);
            }
            break;
        case fanoutSELM_Specified:
            // Check that only target records test_fanout_out(seln+offs) changed
            for (i = 0; i < NRECORDS; ++i) {
                if (i == (c->seln + c->offs)) {
                    testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 1.0);
                } else {
                    testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 0.0);
                }
            }
            break;
        case fanoutSELM_Mask: {
            // Check that only the masked links were processed
            epicsUInt16 expected_bits;
            if (c->shft > 0) {
                expected_bits = c->seln >> c->shft;
            } else {
                expected_bits = c->seln << -c->shft;
            }
            for (i = 0; i < NRECORDS; ++i) {
                if (expected_bits & (1 << i)) {
                    testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 1.0);
                } else {
                    testdbGetFieldEqual(fanout_targets[i], DBF_DOUBLE, 0.0);
                }
            }
            break;
        }
    }
}

MAIN(fanoutTest) {

    testPlan(234);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("fanoutTest.db", NULL, NULL);
    init_target_records();

    eltc(0);
    testIocInitOk();
    eltc(1);

    // Set LNK0-9,LNKA-F in the fanout record to point to each of the
    // calc records used for these tests
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbPutFieldOk(fanout_record_LNK_fields[i], DBF_STRING, fanout_targets[i]);
    }

    run_case(&selm_all);
    run_case(&selm_specified);
    run_case(&selm_specified_upper_invalid);
    run_case(&selm_specified_lower_invalid);
    run_case(&selm_mask);
    run_case(&selm_mask_shift_left);
    run_case(&selm_mask_shift_right);
    run_case(&selm_mask_invalid_shift1);
    run_case(&selm_mask_invalid_shift2);
    run_case(&selm_mask_zero);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
