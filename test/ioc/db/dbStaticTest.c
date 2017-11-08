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

static void verify(DBENTRY *pentry, const char *put, const char *exp)
{
    const char *msg;
    int result;

    msg = dbVerify(pentry, put);
    result = (!msg && !exp) || (msg && exp && strcmp(msg, exp) == 0);

    if (!testOk(result, "dbVerify('%s.%s', '%s') => '%s'",
        (char *) pentry->precnode->precord, pentry->pflddes->name,
        put, msg ? msg : "OK"))
        testDiag("Expected => '%s'", exp ? exp : "OK");
}

static void testDbVerify(const char *record)
{
    DBENTRY entry;

    testDiag("# # # # # # # testDbVerify('%s') # # # # # # # #", record);

    dbInitEntry(pdbbase, &entry);
    if (dbFindRecord(&entry, record) != 0)
        testAbort("Can't find record '%s'", record);

    dbFindField(&entry, "UDF");
    verify(&entry, "0", NULL);
    verify(&entry, "255", NULL);
    verify(&entry, "256", "Number too large for field type");
    verify(&entry, "0x100", "Number too large for field type");

    dbFindField(&entry, "PHAS");
    verify(&entry, "0", NULL);
    verify(&entry, "-32768", NULL);
    verify(&entry, "-32769", "Number too large for field type");
    verify(&entry, "0x7fff", NULL);
    verify(&entry, "32768", "Number too large for field type");

    dbFindField(&entry, "VAL");
    verify(&entry, "0", NULL);
    verify(&entry, "-123456789", NULL);
    verify(&entry, "123456789", NULL);
    verify(&entry, "0x1234FEDC", NULL);
    verify(&entry, "0x100000000", "Number too large for field type");
    verify(&entry, "1.2345", "Extraneous characters after number");

    dbFindField(&entry, "DESC");
    verify(&entry, "", NULL);
    verify(&entry, "abcdefghijklmnopqrstuvwxyz", NULL);
    verify(&entry, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz",
        "String too long, max 40 characters");

    dbFindField(&entry, "DTYP");
    verify(&entry, "Soft Channel", NULL);
    verify(&entry, "zzzz", "Not a valid device type");

    dbFindField(&entry, "SCAN");
    verify(&entry, "1 second", NULL);
    verify(&entry, "zzzz", "Not a valid menu choice");

    dbFindField(&entry, "FLNK");
    verify(&entry, "Anything works here!", NULL);

    dbFinishEntry(&entry);
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbStaticTest)
{
    testPlan(223);
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

    testDbVerify("testrec");

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}

