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

static const char *fanout_receivers[] = {
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

#define NRECORDS NELEMENTS(fanout_receivers)

static calcRecord *receivers[NRECORDS];

static void init_receiver_records(void) {
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        receivers[i] = (calcRecord *)testdbRecordPtr(fanout_receivers[i]);
        if (!receivers[i]) {
            testAbort("Record %s not found", fanout_receivers[i]);
        }
    }
}

static void snapshot(double values[NRECORDS]) {
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        values[i] = receivers[i]->val;
    }
}

static void test_selm_all(void) {
    testDiag("test_selm_all()");
    double before[NRECORDS];
    snapshot(before);

    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "All");
    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i] + 1);
    }
}

static void test_selm_specified() {
    testDiag("test_selm_specified()");
    double before[NRECORDS];
    snapshot(before);

    // Set the SELM, SELN, and OFFS fields and process the fanout record
    const epicsUInt16 seln = 4;
    const epicsInt16 offs = 2;
    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Specified");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.OFFS", DBF_SHORT, offs);

    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Check that only test_fanout_out(seln+offs) changed
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        if (i == (seln + offs)) {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i] + 1);
        } else {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
        }
    }
}

static void test_selm_specified_upper_invalid(void) {
    testDiag("test_selm_specified_upper_invalid()");
    const epicsUInt16 seln = 15;
    const epicsInt16 offs = 1;
    double before[NRECORDS];

    snapshot(before);

    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Specified");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.OFFS", DBF_SHORT, offs);
    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Invalid index must not process any output link.
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
    }

    // Confirm the alarm states are set
    testdbGetFieldEqual("test_fanout_record.STAT", DBF_USHORT, SOFT_ALARM);
    testdbGetFieldEqual("test_fanout_record.SEVR", DBF_USHORT, INVALID_ALARM);
}

static void test_selm_specified_lower_invalid(void) {
    testDiag("test_selm_specified_lower_invalid()");
    const epicsUInt16 seln = 2;
    const epicsInt16 offs = -3;
    double before[NRECORDS];

    snapshot(before);

    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Specified");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.OFFS", DBF_SHORT, offs);
    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Invalid index must not process any output link.
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
    }

    // Confirm the alarm states are set
    testdbGetFieldEqual("test_fanout_record.STAT", DBF_USHORT, SOFT_ALARM);
    testdbGetFieldEqual("test_fanout_record.SEVR", DBF_USHORT, INVALID_ALARM);
}

static void test_selm_mask() {
    testDiag("test_selm_mask()");
    double before[NRECORDS];
    snapshot(before);

    // Set SELN, SHFT, and process the fanout record
    const epicsUInt16 seln = 0x5555; // 0b0101010101010101
    const epicsInt16 shft = 0;
    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Mask");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, shft);

    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Check that only the masked links were processed
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        if (seln & (1 << i)) {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i] + 1);
        } else {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
        }
    }
}

static void test_selm_mask_shift_left() {
    testDiag("test_selm_mask_shift_left()");
    double before[NRECORDS];
    snapshot(before);

    // Set SELN, SHFT, and process the fanout record
    const epicsUInt16 seln = 0x0015; // 0b0000000000010101
    const epicsInt16 shft = -2;
    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Mask");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, shft);

    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Check that only the masked links were processed
    const epicsUInt16 expected_bits = seln << -shft;
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        if (expected_bits & (1 << i)) {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i] + 1);
        } else {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
        }
    }
}

static void test_selm_mask_shift_right() {
    testDiag("test_selm_mask_shift_right()");
    double before[NRECORDS];
    snapshot(before);

    // Set SELN, SHFT, and process the fanout record
    const epicsUInt16 seln = 0xa800; // 0b1010100000000000
    const epicsInt16 shft = 2;
    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Mask");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, shft);

    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Check that only the masked links were processed
    const epicsUInt16 expected_bits = seln >> shft;
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        if (expected_bits & (1 << i)) {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i] + 1);
        } else {
            testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
        }
    }
}

static void test_selm_mask_invalid_shift(epicsInt16 shft) {
    testDiag("test_selm_mask_invalid_shift(%d)", shft);
    const epicsUInt16 seln = 0xffff;
    double before[NRECORDS];

    snapshot(before);

    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Mask");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, shft);
    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // An invalid shift must not process any output link
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbGetFieldEqual(
            fanout_receivers[i], DBF_DOUBLE, before[i]);
    }

    testdbGetFieldEqual("test_fanout_record.STAT", DBF_USHORT, SOFT_ALARM);
    testdbGetFieldEqual("test_fanout_record.SEVR", DBF_USHORT, INVALID_ALARM);
}

static void test_selm_mask_zero() {
    testDiag("test_selm_mask_zero()");
    double before[NRECORDS];
    snapshot(before);

    // Set SELN, SHFT, and process the fanout record
    const epicsUInt16 seln = 0;
    const epicsInt16 shft = 0;
    testdbPutFieldOk("test_fanout_record.SELM", DBF_STRING, "Mask");
    testdbPutFieldOk("test_fanout_record.SELN", DBF_USHORT, seln);
    testdbPutFieldOk("test_fanout_record.SHFT", DBF_SHORT, shft);

    testdbPutFieldOk("test_fanout_record.PROC", DBF_LONG, 1);

    // Check that no links were processed
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbGetFieldEqual(fanout_receivers[i], DBF_DOUBLE, before[i]);
    }
}


MAIN(fanoutTest) {

    testPlan(222);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("fanoutTest.db", NULL, NULL);
    init_receiver_records();

    eltc(0);
    testIocInitOk();
    eltc(1);

    // Set LNK0-9,LNKA-F in the fanout record to point to each of the
    // calc records used for these tests
    int i;
    for (i = 0; i < NRECORDS; ++i) {
        testdbPutFieldOk(fanout_record_LNK_fields[i], DBF_STRING, fanout_receivers[i]);
    }

    test_selm_all();
    test_selm_specified();
    test_selm_mask();
    test_selm_mask_shift_left();
    test_selm_mask_shift_right();
    test_selm_mask_zero();
    test_selm_specified_upper_invalid();
    test_selm_specified_lower_invalid();
    test_selm_mask_invalid_shift(16);
    test_selm_mask_invalid_shift(-16);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
