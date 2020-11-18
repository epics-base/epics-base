/*************************************************************************\
* Copyright (c) 2020 Dirk Zimoch
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "devSup.h"
#include "alarm.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"

#include "longinRecord.h"

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

static void expectProcSuccess(const char *rec)
{
    char fieldname[20];
    testDiag("expecting success from %s", rec);
    sprintf(fieldname, "%s.PROC", rec);
    testdbPutFieldOk(fieldname, DBF_LONG, 1);
    sprintf(fieldname, "%s.SEVR", rec);
    testdbGetFieldEqual(fieldname, DBF_LONG, NO_ALARM);
    sprintf(fieldname, "%s.STAT", rec);
    testdbGetFieldEqual(fieldname, DBF_LONG, NO_ALARM);
}

static void expectProcFailure(const char *rec)
{
    char fieldname[20];
    testDiag("expecting failure S_db_badField %#x from %s", S_db_badField, rec);
    sprintf(fieldname, "%s.PROC", rec);
    testdbPutFieldFail(S_db_onlyOne, fieldname, DBF_LONG, 1);
    sprintf(fieldname, "%s.SEVR", rec);
    testdbGetFieldEqual(fieldname, DBF_LONG, INVALID_ALARM);
    sprintf(fieldname, "%s.STAT", rec);
    testdbGetFieldEqual(fieldname, DBF_LONG, LINK_ALARM);
}

static void changeRange(long start, long stop, long step)
{
    char linkstring[60];
    if (step)
        sprintf(linkstring, "src.[%ld:%ld:%ld]", start, step, stop);
    else if (stop)
        sprintf(linkstring, "src.[%ld:%ld]", start, stop);
    else
        sprintf(linkstring, "src.[%ld]", start);
    testDiag("modifying link: %s", linkstring);
    testdbPutFieldOk("ai.INP", DBF_STRING, linkstring);
    testdbPutFieldOk("wf.INP", DBF_STRING, linkstring);
}

static const double buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 2, 4, 6};

static void expectRange(long start, long end)
{
    long n = end-start+1;
    testdbGetFieldEqual("ai.VAL", DBF_DOUBLE, buf[start]);
    testdbGetFieldEqual("wf.NORD", DBF_LONG, n);
    testdbGetArrFieldEqual("wf.VAL", DBF_DOUBLE, n+2, n, buf+start);
}

static void expectEmptyArray(void)
{
    testDiag("expecting empty array");
    testdbGetFieldEqual("wf.NORD", DBF_LONG, 0);
}

MAIN(linkFilterTest)
{
    testPlan(102);
    startTestIoc("linkFilterTest.db");

    testDiag("PINI");
    expectRange(2,4);

    testDiag("modify range");
    changeRange(3,6,0);
    expectProcSuccess("ai");
    expectProcSuccess("wf");
    expectRange(3,6);

    testDiag("backward range");
    changeRange(5,3,0);
    expectProcFailure("ai");
    expectProcSuccess("wf");
    expectEmptyArray();

    testDiag("step 2");
    changeRange(1,6,2);
    expectProcSuccess("ai");
    expectProcSuccess("wf");
    expectRange(10,12);

    testDiag("range start beyond src.NORD");
    changeRange(8,9,0);
    expectProcFailure("ai");
    expectProcSuccess("wf");
    expectEmptyArray();

    testDiag("range end beyond src.NORD");
    changeRange(3,9,0);
    expectProcSuccess("ai");
    expectProcSuccess("wf");
    expectRange(3,7); /* clipped range */

    testDiag("range start beyond src.NELM");
    changeRange(11,12,0);
    expectProcFailure("ai");
    expectProcSuccess("wf");

    testDiag("range end beyond src.NELM");
    changeRange(4,12,0);
    expectProcSuccess("ai");
    expectProcSuccess("wf");
    expectRange(4,7); /* clipped range */

    testDiag("single value beyond src.NORD");
    changeRange(8,0,0);
    expectProcFailure("ai");
    expectProcSuccess("wf");
    expectEmptyArray();

    testDiag("single value");
    changeRange(5,0,0);
    expectProcSuccess("ai");
    expectProcSuccess("wf");
    expectRange(5,5);

    testDiag("single beyond rec.NELM");
    changeRange(12,0,0);
    expectProcFailure("ai");
    expectProcSuccess("wf");
    expectEmptyArray();

    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
