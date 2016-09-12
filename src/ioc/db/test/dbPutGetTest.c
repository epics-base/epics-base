
#include <string.h>

#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbUnitTest.h>
#include <testMain.h>

static
void testdbGetStringEqual(const char *pv, const char *expected)
{
    const char *actual;
    int ok;
    DBENTRY ent;

    dbInitEntry(pdbbase, &ent);

    if(dbFindRecord(&ent, pv)) {
        testFail("Failed to find record '%s'", pv);
        return;
    }

    actual = dbGetString(&ent);
    ok = (!actual && !expected)
            || (actual && expected && strcmp(actual, expected)==0);

    testOk(ok, "dbGetString(\"%s\") -> \"%s\" == \"%s\"", pv, actual, expected);

    dbFinishEntry(&ent);
}

static
void testGetString(void)
{
    testDiag("testGetString()");

    testdbGetStringEqual("recempty.DTYP", "Soft Channel");
    testdbGetStringEqual("recempty.DESC", "");
    testdbGetStringEqual("recempty.PHAS", "0");
    testdbGetStringEqual("recempty.TSE" , "0");
    testdbGetStringEqual("recempty.DISA", "0");
    testdbGetStringEqual("recempty.DISV", "0");

    testdbGetStringEqual("recoverwrite.DTYP", "Soft Channel");
    testdbGetStringEqual("recoverwrite.DESC", "");
    testdbGetStringEqual("recoverwrite.PHAS", "0");
    testdbGetStringEqual("recoverwrite.TSE" , "0");
    testdbGetStringEqual("recoverwrite.DISA", "0");
    testdbGetStringEqual("recoverwrite.DISV", "0");
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbPutGet)
{
    testPlan(0);
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbPutGetTest.db", NULL, NULL);

    testGetString();

    testdbCleanup();

    return testDone();
}
