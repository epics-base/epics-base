/*************************************************************************\
* Copyright (c) 2017 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "errlog.h"
#include "dbStaticLib.h"
#include "dbUnitTest.h"
#include "testMain.h"

static
void testint64BeforeInit(void)
{
    const char *S;
    DBENTRY dbent;

    /* check dbGet/PutString */

    testDiag("In %s", EPICS_FUNCTION);

    dbInitEntryFromRecord(testdbRecordPtr("out64"), &dbent);
    if(dbFindField(&dbent, "VAL"))
        testAbort("Failed to find out64.VAL");

    S = dbGetString(&dbent);
    testOk(S && strcmp(S, "0")==0, "initial value \"%s\"", S);

    testOk1(dbPutString(&dbent, "0x12345678abcdef00")==0);

    S = dbGetString(&dbent);
    testOk(S && strcmp(S, "1311768467750121216")==0, "1311768467750121216 \"%s\"", S);

    dbFinishEntry(&dbent);
}

static
void testint64AfterInit(void)
{
    testDiag("In %s", EPICS_FUNCTION);

    /* check dbGet/PutField and DB links */

    testdbGetFieldEqual("in64", DBF_UINT64, 0ULL);
    testdbGetFieldEqual("out64", DBF_UINT64, 0x12345678abcdef00ULL);

    testdbPutFieldOk("out64.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("in64", DBF_UINT64, 0x12345678abcdef00ULL);

    testdbPutFieldOk("out64.VAL", DBF_UINT64, 0x22345678abcdef00ULL);

    testdbPutFieldOk("in64.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("in64", DBF_UINT64, 0x22345678abcdef00ULL);
}

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(recMiscTest)
{
    testPlan(10);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("recMiscTest.db", NULL, NULL);

    testint64BeforeInit();

    eltc(0);
    testIocInitOk();
    eltc(1);

    testint64AfterInit();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
