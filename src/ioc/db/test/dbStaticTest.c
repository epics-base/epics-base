#include <string.h>

#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbUnitTest.h>
#include <testMain.h>

static void testEntry(void)
{
    DBENTRY entry;

    testDiag("testEntry");

    dbInitEntry(pdbbase, &entry);

    testOk1(dbFindRecord(&entry, "testrec.VAL")==0);

    testOk1(entry.precordType && strcmp(entry.precordType->name, "x")==0);
    testOk1(entry.precnode && strcmp(((dbCommon*)entry.precnode->precord)->name, "testrec")==0);
    testOk1(entry.pflddes && strcmp(entry.pflddes->name, "VAL")==0);

    testOk1(dbFindInfo(&entry, "A")==0 && strcmp(dbGetInfoString(&entry), "B")==0);

    dbFinishEntry(&entry);
}

static void testAddr2Entry(void)
{
    DBENTRY entry, entry2;
    dbAddr addr;

    testDiag("testAddr2Entry");

    memset(&entry, 0, sizeof(entry));
    memset(&entry2, 0, sizeof(entry2));

    dbInitEntry(pdbbase, &entry);

    if(dbFindRecord(&entry, "testrec.VAL")!=0)
        testAbort("no entry for testrec.VAL");

    if(dbNameToAddr("testrec.VAL", &addr))
        testAbort("no addr for testrec.VAL");

    dbInitEntryFromAddr(&addr, &entry2);

    testOk1(entry.pdbbase==entry2.pdbbase);
    testOk1(entry.precordType==entry2.precordType);
    testOk1(entry.pflddes==entry2.pflddes);
    testOk1(entry.precnode==entry2.precnode);
    testOk1(entry.pfield==entry2.pfield);
    testOk1(entry2.indfield==-1);
    testOk1(!entry2.pinfonode);
    testOk1(!entry2.message);

    /* no way to look this up, so not set */
    entry.indfield = -1;

    testOk1(memcmp(&entry, &entry2, sizeof(entry))==0);

    dbFinishEntry(&entry);
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbStaticTest)
{
    testPlan(19);
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbStaticTest.db", NULL, NULL);

    testEntry();

    eltc(0);
    testIocInitOk();
    eltc(1);

    testEntry();
    testAddr2Entry();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}

