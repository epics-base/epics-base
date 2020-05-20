/*************************************************************************\
* Copyright (c) 2020 Gabriel Fedel
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
    testPlan(4*16);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("seqTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testSeqSpecified();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
