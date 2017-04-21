/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/


#define EPICS_DBCA_PRIVATE_API

#include <string.h>

#include "dbAccess.h"
#include "alarm.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"

#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void testLongStringInit()
{
    testDiag("testLongStringInit");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("linkInitTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    {
        const char buf[] = "!----------------------------------------------!";
        testdbGetArrFieldEqual("longstr1.VAL$", DBF_CHAR, NELEMENTS(buf)+2, NELEMENTS(buf), buf);
        testdbGetFieldEqual("longstr1.VAL", DBR_STRING, "!--------------------------------------");
    }

    {
        const char buf[] = "!----------------------------------------------!";
        testdbGetArrFieldEqual("longstr2.VAL$", DBF_CHAR, NELEMENTS(buf)+2, NELEMENTS(buf), buf);
        testdbGetFieldEqual("longstr2.VAL", DBR_STRING, "!--------------------------------------");
    }

    {
        const char buf[] = "!----------------------------------------------!";
        testdbGetArrFieldEqual("longstr3.VAL$", DBF_CHAR, NELEMENTS(buf)+2, NELEMENTS(buf), buf);
        testdbGetFieldEqual("longstr3.VAL", DBR_STRING, "!--------------------------------------");
    }

    testIocShutdownOk();

    testdbCleanup();
}

static void testCalcInit()
{
    testDiag("testCalcInit");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("linkInitTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testdbGetFieldEqual("emptylink.VAL", DBR_DOUBLE, 0.0);
    testdbGetFieldEqual("emptylink.SEVR", DBR_LONG, INVALID_ALARM);

    testdbPutFieldOk("emptylink.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("emptylink.VAL", DBR_DOUBLE, 0.0);
    testdbGetFieldEqual("emptylink.SEVR", DBR_LONG, 0);

    testdbGetFieldEqual("emptylink1.VAL", DBR_DOUBLE, 0.0);
    testdbGetFieldEqual("emptylink1.SEVR", DBR_LONG, INVALID_ALARM);

    testdbPutFieldOk("emptylink1.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("emptylink1.VAL", DBR_DOUBLE, 1.0);
    testdbGetFieldEqual("emptylink1.SEVR", DBR_LONG, 0);

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(linkInitTest)
{
    testPlan(16);
    testLongStringInit();
    testCalcInit();
    return testDone();
}
