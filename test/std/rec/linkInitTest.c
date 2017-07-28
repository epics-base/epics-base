/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "alarm.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"

#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void startTestIoc(const char *dbfile)
{
    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase(dbfile, NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
}

static void testLongStringInit()
{
    testDiag("testLongStringInit");

    startTestIoc("linkInitTest.db");

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

    startTestIoc("linkInitTest.db");

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

static void testPrintfStrings()
{
    testDiag("testPrintfStrings");

    startTestIoc("linkInitTest.db");

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

static void testArrayInputs()
{
    epicsInt32 oneToTwelve[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    testDiag("testArrayInputs");

    startTestIoc("linkInitTest.db");

    testdbGetFieldEqual("aai1.NORD", DBR_LONG, 10);
    testdbGetFieldEqual("aai1.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("aai2.NORD", DBR_LONG, 10);
    testdbGetFieldEqual("aai2.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("sa1.NORD", DBR_LONG, 10);
    testdbGetFieldEqual("sa1.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("sa2.NORD", DBR_LONG, 0);
    testdbGetFieldEqual("sa2.UDF", DBR_UCHAR, 1);
    testdbGetFieldEqual("wf1.NORD", DBR_LONG, 10);
    testdbGetFieldEqual("wf1.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("wf2.NORD", DBR_LONG, 10);
    testdbGetFieldEqual("wf2.UDF", DBR_UCHAR, 0);

    testdbGetArrFieldEqual("aai1.VAL", DBF_LONG, 12, 10, &oneToTwelve[0]);
    testdbGetArrFieldEqual("aai2.VAL", DBF_LONG, 12, 10, &oneToTwelve[0]);
    testdbGetArrFieldEqual("sa1.VAL", DBF_LONG, 12, 10, &oneToTwelve[2]);
    testdbGetArrFieldEqual("sa2.VAL", DBF_LONG, 10, 0, NULL);
    testdbGetArrFieldEqual("wf1.VAL", DBF_LONG, 12, 10, &oneToTwelve[0]);
    testdbGetArrFieldEqual("wf2.VAL", DBF_LONG, 12, 10, &oneToTwelve[0]);

    testdbPutFieldOk("sa1.INDX", DBF_LONG, 3);
    testdbGetArrFieldEqual("sa1.VAL", DBF_LONG, 12, 9, &oneToTwelve[3]);

    testdbPutFieldOk("sa1.NELM", DBF_LONG, 3);
    testdbGetArrFieldEqual("sa1.VAL", DBF_LONG, 12, 3, &oneToTwelve[3]);

    testdbPutFieldOk("sa2.VAL", DBF_LONG, 1);
    testdbGetArrFieldEqual("sa2.VAL", DBF_LONG, 10, 1, &oneToTwelve[0]);

    testDiag("testScalarInputs");

    testdbGetFieldEqual("li1", DBR_LONG, 1);
    testdbGetFieldEqual("i64i1", DBR_INT64, 1LL);
    testdbGetFieldEqual("li2", DBR_LONG, 1);
    testdbGetFieldEqual("i64i2", DBR_INT64, 1LL);

    testIocShutdownOk();
    testdbCleanup();
}

static void testEventRecord()
{
    testMonitor *countmon;

    testDiag("testEventRecord");

    startTestIoc("linkInitTest.db");
    countmon = testMonitorCreate("count1.VAL", DBR_LONG, 0);

    testdbGetFieldEqual("ev1.VAL", DBR_STRING, "soft event 1");
    testdbGetFieldEqual("ev1.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("ev2.VAL", DBR_STRING, "");
    testdbGetFieldEqual("ev2.UDF", DBR_UCHAR, 1);
    testdbGetFieldEqual("count1.VAL", DBR_LONG, 0);

    testdbPutFieldOk("ev1.PROC", DBF_UCHAR, 1);
    testMonitorWait(countmon);
    testdbGetFieldEqual("count1.VAL", DBR_LONG, 1);

    testdbPutFieldOk("ev2.PROC", DBF_UCHAR, 1);
    testMonitorWait(countmon);
    testdbGetFieldEqual("ev2.UDF", DBR_UCHAR, 0);
    testdbGetFieldEqual("count1.VAL", DBR_LONG, 2);

    testdbPutFieldOk("count1.EVNT", DBF_STRING, "Tock");
    testdbPutFieldOk("ev2.PROC", DBF_UCHAR, 1);
    testMonitorWait(countmon);
    testdbGetFieldEqual("count1.VAL", DBR_LONG, 3);

    testMonitorDestroy(countmon);
    testIocShutdownOk();
    testdbCleanup();
}


MAIN(linkInitTest)
{
    testPlan(72);

    testLongStringInit();
    testCalcInit();
    testPrintfStrings();
    testArrayInputs();
    testEventRecord();

    return testDone();
}
