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

    testdbGetFieldEqual("longstr4.VAL", DBR_STRING, "One");

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

static void testPrintfInit()
{
    testDiag("testPrintfInit");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("linkInitTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    {
        const char buf1[] = "Test string, exactly 40 characters long";
        const char buf2[] = "Longer test string, more that 40 characters long";
        const char buf2t[] = "Longer test string, more that 40 charac";

        /* The FMT field is pp(TRUE), so this put triggers processing */
        testdbPutFieldOk("printf1.FMT", DBF_STRING, "%s");
        testdbGetArrFieldEqual("printf1.VAL$", DBF_CHAR, NELEMENTS(buf1)+2,
            NELEMENTS(buf1), buf1);
        testdbGetFieldEqual("printf1.VAL", DBR_STRING, buf1);

        testdbPutFieldOk("printf1.FMT", DBF_STRING, "%ls");
        testdbGetArrFieldEqual("printf1.VAL$", DBF_CHAR, NELEMENTS(buf1)+2,
            NELEMENTS(buf1), buf1);
        testdbGetFieldEqual("printf1.VAL", DBR_STRING, buf1);

        testdbPutFieldOk("printf2.FMT", DBF_STRING, "%s");
        testdbGetArrFieldEqual("printf2.VAL$", DBF_CHAR, NELEMENTS(buf2)+2,
            NELEMENTS(buf2t), buf2t);
        testdbGetFieldEqual("printf2.VAL", DBR_STRING, buf2t);

        testdbPutFieldOk("printf2.FMT", DBF_STRING, "%ls");
        testdbGetArrFieldEqual("printf2.VAL$", DBF_CHAR, NELEMENTS(buf2)+2,
            NELEMENTS(buf2), buf2);
        testdbGetFieldEqual("printf2.VAL", DBR_STRING, buf2t);

        testdbPutFieldOk("printf2.FMT", DBF_STRING, "%.39ls");
        testdbGetArrFieldEqual("printf2.VAL$", DBF_CHAR, NELEMENTS(buf2)+2,
            NELEMENTS(buf2t), buf2t);
    }

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(linkInitTest)
{
    testPlan(31);
    testLongStringInit();
    testCalcInit();
    testPrintfInit();
    return testDone();
}
