#include <string.h>

#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbUnitTest.h>
#include <testMain.h>

static void testEntry(const char *pv)
{
    DBENTRY entry;

    testDiag("testEntry(\"%s\")", pv);

    dbInitEntry(pdbbase, &entry);

    testOk1(dbFindRecord(&entry, pv)==0);

    testDiag("precordType=%p precnode=%p", entry.precordType, entry.precnode);
    testOk(entry.precordType &&
        strcmp(entry.precordType->name, "x")==0,
        "Record type is '%s' ('x')", entry.precordType->name);
    testOk(entry.precnode &&
        strcmp(((dbCommon*)entry.precnode->precord)->name, "testrec")==0,
        "Record name is '%s' ('testrec')", ((dbCommon*)entry.precnode->precord)->name);
    testOk(entry.pflddes &&
        strcmp(entry.pflddes->name, "VAL")==0,
        "Field name is '%s' ('VAL')", entry.pflddes->name);

    /* all tested PVs are either a record with aliases, or an alias */
    testOk(entry.precnode &&
        (!(entry.precnode->flags & DBRN_FLAGS_ISALIAS) ^
         !(entry.precnode->flags & DBRN_FLAGS_HASALIAS)),
        "Recnode flags %d", entry.precnode->flags);

    testOk1(dbFollowAlias(&entry)==0);

    testOk(dbFindInfo(&entry, "A")==0 &&
        strcmp(dbGetInfoString(&entry), "B")==0,
        "Info item is set");

    dbFinishEntry(&entry);
}

static void testAddr2Entry(const char *pv)
{
    DBENTRY entry, entry2;
    dbAddr addr;

    testDiag("testAddr2Entry(\"%s\")", pv);

    memset(&entry, 0, sizeof(entry));
    memset(&entry2, 0, sizeof(entry2));

    dbInitEntry(pdbbase, &entry);

    if(dbFindRecord(&entry, pv)!=0)
        testAbort("no entry for %s", pv);

    if(dbFollowAlias(&entry))
        testAbort("Can't follow alias");

    if(dbNameToAddr(pv, &addr))
        testAbort("no addr for %s", pv);

    dbInitEntryFromAddr(&addr, &entry2);

    testOk1(entry.pdbbase==entry2.pdbbase);
    testOk1(entry.precordType==entry2.precordType);
    testOk1(entry.pflddes==entry2.pflddes);
    testOk1(entry.precnode==entry2.precnode);
    testOk1(entry.pfield==entry2.pfield);
    testOk1(entry.indfield==entry2.indfield);
    testOk1(!entry2.pinfonode);
    testOk1(!entry2.message);

    testOk(memcmp(&entry, &entry2, sizeof(entry))==0, "dbEntries identical");

    dbFinishEntry(&entry);
    dbFinishEntry(&entry2);
}

static void testRec2Entry(const char *recname)
{
    DBENTRY entry, entry2;
    dbCommon *prec;

    testDiag("testRec2Entry(\"%s\")", recname);

    memset(&entry, 0, sizeof(entry));
    memset(&entry2, 0, sizeof(entry2));

    prec = testdbRecordPtr(recname);

    dbInitEntry(pdbbase, &entry);

    if(dbFindRecord(&entry, recname)!=0)
        testAbort("no entry for %s", recname);

    if(dbFollowAlias(&entry))
        testAbort("Can't follow alias");

    dbInitEntryFromRecord(prec, &entry2);

    testOk1(entry.pdbbase==entry2.pdbbase);
    testOk1(entry.precordType==entry2.precordType);
    testOk1(entry.pflddes==entry2.pflddes);
    testOk1(entry.precnode==entry2.precnode);
    testOk1(entry.pfield==entry2.pfield);
    testOk1(entry.indfield==entry2.indfield);
    testOk1(!entry2.pinfonode);
    testOk1(!entry2.message);

    testOk(memcmp(&entry, &entry2, sizeof(entry))==0, "dbEntries identical");

    dbFinishEntry(&entry);
    dbFinishEntry(&entry2);
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbStaticTest)
{
    testPlan(200);
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbStaticTest.db", NULL, NULL);

    testEntry("testrec.VAL");
    testEntry("testalias.VAL");
    testEntry("testalias2.VAL");
    testEntry("testalias3.VAL");
    testAddr2Entry("testrec.VAL");
    testAddr2Entry("testalias.VAL");
    testAddr2Entry("testalias2.VAL");
    testAddr2Entry("testalias3.VAL");
    testRec2Entry("testrec");
    testRec2Entry("testalias");
    testRec2Entry("testalias2");
    testRec2Entry("testalias3");

    eltc(0);
    testIocInitOk();
    eltc(1);

    testEntry("testrec.VAL");
    testEntry("testalias.VAL");
    testEntry("testalias2.VAL");
    testEntry("testalias3.VAL");
    testAddr2Entry("testrec.VAL");
    testAddr2Entry("testalias.VAL");
    testAddr2Entry("testalias2.VAL");
    testAddr2Entry("testalias3.VAL");
    testRec2Entry("testrec");
    testRec2Entry("testalias");
    testRec2Entry("testalias2");
    testRec2Entry("testalias3");

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}

