/*************************************************************************\
* Copyright (c) 2020 Gabriel Fedel
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"
#include <epicsThread.h>

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

/*
 * The DLYn fields advertise a display range of 0 to 10 seconds
 * */
static
void testSeqDlyLimits(void){
    DBADDR addr;
    struct {
        DBRgrDouble
        epicsFloat64 value;
    } buf;
    long options = DBR_GR_DOUBLE;
    long nRequest = 1;

    testOk1(!dbNameToAddr("seq0.DLY0", &addr));
    testOk1(!dbGetField(&addr, DBR_DOUBLE, &buf, &options, &nRequest, NULL));
    testOk(buf.lower_disp_limit == 0.0,
        "DLY0 lower_disp_limit (%g) == 0", buf.lower_disp_limit);
    testOk(buf.upper_disp_limit == 10.0,
        "DLY0 upper_disp_limit (%g) == 10", buf.upper_disp_limit);
}

/*
 * This test verifies the behavior of seq using Specified for SELM
 * The behavior should be the same for all the DOLx
 * */
static
void testSeqSpecified(void){
    int i;
    for (i=0; i < 16; i++) {
        testdbPutFieldOk("seq0.SELN", DBR_USHORT, i);

        testdbPutFieldOk("ai0", DBR_LONG, 0);

        testdbPutFieldOk("seq0.PROC", DBR_USHORT, 1);

        testSyncCallback();
        testdbGetFieldEqual("ai0", DBR_LONG, i+1);
    }
}


MAIN(eventTest) {
    testPlan(4*16 + 4);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("seqTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testSeqDlyLimits();
    testSeqSpecified();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
