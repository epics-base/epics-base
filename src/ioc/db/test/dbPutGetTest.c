
#include <string.h>

#include <errlog.h>
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

static
void testStringMax(void)
{
    testDiag("testStringMax()");

    testdbGetStringEqual("recmax.DISA", "-1");
}

static
void testLongLink(void)
{
    testDiag("testLonkLink()");

    testdbGetFieldEqual("lnktest.INP", DBR_STRING, "lnktarget NPP NMS");
    testdbGetFieldEqual("lnktest.INP$", DBR_STRING, "lnktarget NPP NMS");
    testDiag("dbGet() w/ nRequest==1 gets only trailing nil");
    testdbGetFieldEqual("lnktest.INP$", DBR_CHAR, '\0');
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 19, 18, "lnktarget NPP NMS");

    testDiag("get w/ truncation");
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 0, 0, NULL);
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 1, 1, "");
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 2, 2, "l");
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 3, 3, "ln");
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 17, 17, "lnktarget NPP NM");
    testdbGetArrFieldEqual("lnktest.INP$", DBR_CHAR, 18, 18, "lnktarget NPP NMS");

    testdbGetArrFieldEqual("lnktest.INP", DBR_STRING, 2, 1, "lnktarget NPP NMS");
}

static
void testLongAttr(void)
{
    testDiag("testLongAttr()");

    testdbGetFieldEqual("lnktest.RTYP", DBR_STRING, "x");
    testdbGetFieldEqual("lnktest.RTYP$", DBR_STRING, "x");
    testDiag("dbGet() w/ nRequest==1 gets only trailing nil");
    testdbGetFieldEqual("lnktest.RTYP$", DBR_CHAR, '\0');

    testdbGetArrFieldEqual("lnktest.RTYP$", DBR_CHAR, 4, 2, "x");

    testDiag("get w/ truncation");
    testdbGetArrFieldEqual("lnktest.RTYP$", DBR_CHAR, 0, 0, NULL);
    testdbGetArrFieldEqual("lnktest.RTYP$", DBR_CHAR, 1, 1, "");
    testdbGetArrFieldEqual("lnktest.RTYP$", DBR_CHAR, 2, 2, "x");
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbPutGet)
{
    testPlan(31);
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbPutGetTest.db", NULL, NULL);

    testGetString();

    testStringMax();

    eltc(0);
    testIocInitOk();
    eltc(1);

    testLongLink();
    testLongAttr();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
