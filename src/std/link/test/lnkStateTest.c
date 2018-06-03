/*************************************************************************\
* Copyright (c) 2018 Andrew Johnson
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "alarm.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"
#include "dbLink.h"
#include "dbState.h"
#include "ioRecord.h"

#include "testMain.h"

void linkTest_registerRecordDeviceDriver(struct dbBase *);

static void startTestIoc(const char *dbfile)
{
    testdbPrepare();
    testdbReadDatabase("linkTest.dbd", NULL, NULL);
    linkTest_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase(dbfile, NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
}

static void testState()
{
    dbStateId red;
    ioRecord *pio;
    DBLINK *pinp, *pout;
    long status;

    testDiag("testing lnkState");

    startTestIoc("ioRecord.db");

    pio = (ioRecord *) testdbRecordPtr("io");
    pinp = &pio->input;
    pout = &pio->output;

    red = dbStateFind("red");
    testOk(!red, "No state red exists");

    testdbPutFieldOk("io.INPUT", DBF_STRING, "{\"state\":\"red\"}");
    if (testOk1(pinp->type == JSON_LINK))
        testDiag("Link was set to '%s'", pinp->value.json.string);
    red = dbStateFind("red");
    testOk(!!red, "state red exists");

    {
        epicsInt16 i16;

        dbStateSet(red);
        status = dbGetLink(pinp, DBF_SHORT, &i16, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(i16, "Got TRUE");

        testdbPutFieldOk("io.INPUT", DBF_STRING, "{\"state\":\"!red\"}");
        if (testOk1(pinp->type == JSON_LINK))
            testDiag("Link was set to '%s'", pinp->value.json.string);

        status = dbGetLink(pinp, DBF_SHORT, &i16, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(!i16, "Got FALSE");

        testdbPutFieldOk("io.OUTPUT", DBF_STRING, "{\"state\":\"red\"}");
        if (testOk1(pout->type == JSON_LINK))
            testDiag("Link was set to '%s'", pout->value.json.string);

        i16 = 0;
        status = dbPutLink(pout, DBF_SHORT, &i16, 1);
        testOk(!status, "dbPutLink %d succeeded (status = %ld)", i16, status);
        testOk(!dbStateGet(red), "state was cleared");

        i16 = 0x8000;
        status = dbPutLink(pout, DBF_SHORT, &i16, 1);
        testOk(!status, "dbPutLink 0x%hx succeeded (status = %ld)", i16, status);
        testOk(dbStateGet(red), "state was set");
    }

    status = dbPutLink(pout, DBF_STRING, "", 1);
    testOk(!status, "dbPutLink '' succeeded (status = %ld)", status);
    testOk(!dbStateGet(red), "state was cleared");

    status = dbPutLink(pout, DBF_STRING, "FALSE", 1); /* Not really... */
    testOk(!status, "dbPutLink 'FALSE' succeeded (status = %ld)", status);
    testOk(dbStateGet(red), "state was set");

    status = dbPutLink(pout, DBF_STRING, "0", 1);
    testOk(!status, "dbPutLink '0' succeeded (status = %ld)", status);
    testOk(!dbStateGet(red), "state was cleared");

    {
        epicsFloat64 f64 = 0.1;

        status = dbPutLink(pout, DBF_DOUBLE, &f64, 1);
        testOk(!status, "dbPutLink %g succeeded (status = %ld)", f64, status);
        testOk(dbStateGet(red), "state was set");

        testdbPutFieldOk("io.OUTPUT", DBF_STRING, "{\"state\":\"!red\"}");
        if (testOk1(pout->type == JSON_LINK))
            testDiag("Link was set to '%s'", pout->value.json.string);

        status = dbPutLink(pout, DBF_DOUBLE, &f64, 1);
        testOk(!status, "dbPutLink %g succeeded (status = %ld)", f64, status);
        testOk(!dbStateGet(red), "state was cleared");
    }

    testIocShutdownOk();

    testdbCleanup();
}


MAIN(lnkStateTest)
{
    testPlan(28);

    testState();

    return testDone();
}
