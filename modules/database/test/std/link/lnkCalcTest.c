/*************************************************************************\
* Copyright (c) 2018 Andrew Johnson
* SPDX-License-Identifier: EPICS
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
#include "recGbl.h"
#include "testMain.h"
#include "ioRecord.h"

#define testPutLongStr(PV, VAL) \
    testdbPutArrFieldOk(PV, DBF_CHAR, sizeof(VAL), VAL);

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

static void testCalc()
{
    ioRecord *pio;
    DBLINK *pinp, *pout;
    long status;
    epicsFloat64 f64;

    startTestIoc("ioRecord.db");

    pio = (ioRecord *) testdbRecordPtr("io");
    pinp = &pio->input;
    pout = &pio->output;

    testDiag("testing lnkCalc input");

    {
        dbStateId red;

        testPutLongStr("io.INPUT", "{calc:{"
            "expr:'a',"
            "args:[{state:'red'}]"
            "}}");
        if (testOk1(pinp->type == JSON_LINK))
            testDiag("Link was set to '%s'", pinp->value.json.string);
        red = dbStateFind("red");
        testOk(!!red, "State red was created");

        dbStateSet(red);
        status = dbGetLink(pinp, DBF_DOUBLE, &f64, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(f64, "Got TRUE (%g)", f64);

        dbStateClear(red);
        status = dbGetLink(pinp, DBF_DOUBLE, &f64, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(!f64, "Got FALSE (%g)", f64);
    }

    {
        dbStateId major = dbStateCreate("major");
        dbStateId minor = dbStateCreate("minor");
        epicsEnum16 stat, sevr;

        testPutLongStr("io.INPUT", "{calc:{"
            "expr:'0',"
            "major:'A',"
            "minor:'B',"
            "args:[{state:'major'},{state:'minor'}]"
            "}}");
        if (testOk1(pinp->type == JSON_LINK))
            testDiag("Link was set to '%s'", pinp->value.json.string);

        dbStateSet(major);
        dbStateSet(minor);
        status = dbGetLink(pinp, DBF_DOUBLE, &f64, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(f64 == 0.0, "Got zero (%g)", f64);
        testOk(recGblResetAlarms(pio) & DBE_ALARM, "Record alarm was raised");
        status = dbGetAlarm(pinp, &stat, &sevr);
        testOk(!status, "dbGetAlarm succeeded (status = %ld)", status);
        testOk(stat == LINK_ALARM, "Alarm status = LINK (%d)", stat);
        testOk(sevr == MAJOR_ALARM, "Alarm severity = MAJOR (%d)", sevr);

        dbStateClear(major);
        status = dbGetLink(pinp, DBF_DOUBLE, &f64, NULL, NULL);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(recGblResetAlarms(pio) & DBE_ALARM, "Record alarm was raised");
        status = dbGetAlarm(pinp, &stat, &sevr);
        testOk(!status, "dbGetAlarm succeeded (status = %ld)", status);
        testOk(stat == LINK_ALARM, "Alarm status = LINK (%d)", stat);
        testOk(sevr == MINOR_ALARM, "Alarm severity = MINOR (%d)", sevr);
    }

    testDiag("testing lnkCalc output");

    {
        dbStateId red = dbStateFind("red");
        dbStateId out = dbStateCreate("out");

        testPutLongStr("io.OUTPUT", "{calc:{"
            "expr:'!a',"
            "out:{state:'out'},"
            "args:[{state:'red'}],"
            "units:'things',"
            "prec:3"
            "}}");
        if (testOk1(pout->type == JSON_LINK))
            testDiag("Link was set to '%s'", pout->value.json.string);

        dbStateSet(red);
        f64 = 1.0;
        status = dbPutLink(pout, DBF_DOUBLE, &f64, 1);
        testOk(!status, "dbPutLink succeeded (status = %ld)", status);
        testOk(!dbStateGet(out), "output was cleared");

        dbStateClear(red);
        status = dbPutLink(pout, DBF_DOUBLE, &f64, 1);
        testOk(!status, "dbGetLink succeeded (status = %ld)", status);
        testOk(dbStateGet(out), "output was set");
    }

    {
        char units[20] = {0};
        short prec = 0;

        status = dbGetUnits(pout, units, sizeof(units));
        testOk(!status, "dbGetUnits succeeded (status = %ld)", status);
        testOk(!strcmp(units, "things"), "Units string correct (%s)", units);

        status = dbGetPrecision(pout, &prec);
        testOk(!status, "dbGetPrecision succeeded (status = %ld)", status);
        testOk(prec == 3, "Precision correct (%d)", prec);
    }

    testIocShutdownOk();

    testdbCleanup();
}


MAIN(lnkCalcTest)
{
    testPlan(30);

    testCalc();

    return testDone();
}
