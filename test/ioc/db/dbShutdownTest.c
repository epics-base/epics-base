/*************************************************************************\
* Copyright (c) 2013 Brookhaven National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "iocInit.h"
#include "dbBase.h"
#include "dbAccess.h"
#include "registry.h"
#include "dbStaticLib.h"
#include "osiFileName.h"
#include "dbmf.h"
#include "errlog.h"

#include "testMain.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static struct threadItem {
    char *name;
    char found;
} commonThreads[] = {
    { "errlog", 0 },
    { "taskwd", 0 },
    { "timerQueue", 0 },
    { "cbLow", 0 },
    { "scanOnce", 0 },
    { NULL, 0 }
};

static
void findCommonThread (epicsThreadId id) {
    struct threadItem *thr;
    char name[32];

    epicsThreadGetName(id, name, 32);

    for (thr = commonThreads; thr->name; thr++) {
        if (epicsStrCaseCmp(thr->name, name) == 0) {
            thr->found = 1;
        }
    }
}

static
void checkCommonThreads (void) {
    struct threadItem *thr;

    for (thr = commonThreads; thr->name; thr++) {
        testOk(thr->found, "Thread %s is running", thr->name);
        thr->found = 0;
    }
}

static
void cycle(void) {

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    epicsThreadMap(findCommonThread);
    checkCommonThreads();

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(dbShutdownTest)
{
    testPlan(10);

    cycle();
    cycle();

    return testDone();
}
