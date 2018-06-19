/*************************************************************************\
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include "dbCaTest.h"
#include "dbAccess.h"

#include "dbUnitTest.h"
#include "testMain.h"

#include "dbAccess.h"
#include "errlog.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static
void testCaStats(void) {
    int channels;
    int disconnected;

    testDiag("Check dbcaStats");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbCaStats.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);


    testDiag("No CA links");

    channels = disconnected = -1;
    dbcaStats(&channels, &disconnected);

    testOk(channels==0, "channels==0 (got %d)", channels);
    testOk(disconnected==0, "disconnected==0 (got %d)", disconnected);


    testDiag("One CA link, disconnected");

    testdbPutFieldOk("e2.INP", DBR_STRING, "e12 CA", 1);

    channels = disconnected = -1;
    dbcaStats(&channels, &disconnected);

    testOk(channels==1, "channels==1 (got %d)", channels);
    testOk(disconnected==1, "disconnected==1 (got %d)", disconnected);

    /* Connected CA links can not be tested without fully starting the IOC
       which we will skip for the moment as it is not allowed inside the vxWorks/RTEMS test harness.
       The above is good enough to check for bug lp:1394212 */

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(dbCaStatsTest)
{
    testPlan(5);
    testCaStats();
    return testDone();
}
