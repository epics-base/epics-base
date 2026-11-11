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

#include "alarm.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

#define SUBARRAY_MALM 6

static void test_indexing(epicsUInt32 nelm, epicsUInt32 indx, epicsInt32 expected_count, const epicsInt32 *expected) {
    testdbPutFieldOk("test_subarray_record.NELM", DBF_ULONG, nelm); // process-passive
    testdbPutFieldOk("test_subarray_record.INDX", DBF_ULONG, indx); // process-passive
    testdbGetFieldEqual("test_subarray_record.NORD", DBF_LONG, expected_count);
    testdbGetArrFieldEqual("test_subarray_record.VAL", DBF_LONG, SUBARRAY_MALM, expected_count, expected);
}

MAIN(subArrayTest) {

    testPlan(36);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("subArrayTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    epicsInt32 all[] = {11, 22, 33, 44, 55, 66};
    epicsInt32 first_three[] = {11, 22, 33};
    epicsInt32 last_three[] = {44, 55, 66};
    epicsInt32 truncated[] = {55, 66};
    epicsInt32 last[] = {66};

    test_indexing(6, 0, 6, all);
    test_indexing(3, 0, 3, first_three);
    test_indexing(3, 3, 3, last_three);
    test_indexing(4, 4, 2, truncated);

    test_indexing(0, 0, 0, NULL);
    testdbGetFieldEqual("test_subarray_record.UDF", DBF_UCHAR, TRUE);
    testdbGetFieldEqual("test_subarray_record.STAT", DBF_USHORT, UDF_ALARM);
    testdbGetFieldEqual("test_subarray_record.SEVR", DBF_USHORT, INVALID_ALARM);

    // When setting INDX >= MALM, it is clamped to MALM - 1
    test_indexing(1, SUBARRAY_MALM, 1, last);
    testdbGetFieldEqual("test_subarray_record.INDX", DBF_ULONG, (epicsUInt32)(SUBARRAY_MALM - 1));

    // When setting NELM > MALM, it is clamped to MALM
    test_indexing(SUBARRAY_MALM + 1, 0, SUBARRAY_MALM, all);
    testdbGetFieldEqual("test_subarray_record.NELM", DBF_ULONG, (epicsUInt32)SUBARRAY_MALM);

    // Confirm alarms are cleared after the previous valid writes
    testdbGetFieldEqual("test_subarray_record.UDF", DBF_UCHAR, FALSE);
    testdbGetFieldEqual("test_subarray_record.STAT", DBF_USHORT, NO_ALARM);
    testdbGetFieldEqual("test_subarray_record.SEVR", DBF_USHORT, NO_ALARM);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
