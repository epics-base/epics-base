/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include <string.h>

#include <errlog.h>
#include <alarm.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <dbUnitTest.h>
#include <testMain.h>

#include "xRecord.h"

typedef struct {
    DBRstatus
    DBRamsg
    DBRunits
    DBRprecision
    DBRtime
    DBRutag
    DBRgrDouble
    DBRctrlDouble
    DBRalDouble
} dbMetaDouble;

enum {dbMetaDoubleMask = DBR_STATUS | DBR_AMSG | DBR_UNITS | DBR_PRECISION | DBR_TIME | DBR_UTAG | DBR_GR_DOUBLE | DBR_CTRL_DOUBLE | DBR_AL_DOUBLE};

static
void testdbMetaDoubleSizes(void)
{
    dbMetaDouble meta;
    size_t pos=0;

    testDiag("dbMetaDouble may not have padding");
#define testOffset(FLD) do {\
    testOk(offsetof(dbMetaDouble, FLD)==pos, "offset(meta, " #FLD "), %u == %u", (unsigned)offsetof(dbMetaDouble, FLD), (unsigned)pos); \
    pos += sizeof(meta.FLD); \
}while(0)

    testOffset(status);
    testOffset(severity);
    testOffset(acks);
    testOffset(ackt);
    testOffset(amsg);
    testOffset(units);
    testOffset(precision);
    testOffset(time);
    testOffset(utag);
    testOffset(upper_disp_limit);
    testOffset(lower_disp_limit);
    testOffset(upper_ctrl_limit);
    testOffset(lower_ctrl_limit);
    testOffset(upper_alarm_limit);
    testOffset(upper_warning_limit);
    testOffset(lower_warning_limit);
    testOffset(lower_alarm_limit);
#undef testOffset
    testOk(sizeof(dbMetaDouble)==pos, "sizeof(dbMetaDouble), %u == %u", (unsigned)sizeof(dbMetaDouble), (unsigned)pos);
}

static
void checkDoubleGet(dbChannel *chan, db_field_log* pfl)
{
    dbMetaDouble meta;
    long options = (long)dbMetaDoubleMask;
    long nReq = 0;
    long status;

    status=dbChannelGet(chan, DBF_DOUBLE, &meta, &options, &nReq, pfl);
    testOk(status==0, "dbGet OTST : %ld", status);

    testOk1(meta.severity==INVALID_ALARM);
    testOk1(meta.status==UDF_ALARM);
    testOk1(meta.acks==MAJOR_ALARM);
    testOk1(meta.ackt==1);
    testOk1(strncmp(meta.amsg, "oops", DB_UNITS_SIZE)==0);
    testOk1(meta.time.secPastEpoch==0x12345678);
    testOk1(meta.time.nsec==0x90abcdef);
    testOk1(meta.utag==0x10203040);
    testOk1(meta.precision.dp==0x12345678);
    testOk1(strncmp(meta.units, "arbitrary", DB_UNITS_SIZE)==0);
#define limitEq(UL, FL, VAL) testOk(meta.UL ## _ ## FL ## _limit == (VAL), #UL "_" #FL "_limit (%f) == %f", meta.UL ## _ ## FL ## _limit, VAL)
    limitEq(lower, disp, 10000000.0-1.0);
    limitEq(upper, disp, 10000000.0+1.0);
    limitEq(lower, ctrl, 10000000.0-2.0);
    limitEq(upper, ctrl, 10000000.0+2.0);
    limitEq(lower, alarm, 10000000.0-3.0);
    limitEq(lower, warning, 10000000.0-4.0);
    limitEq(upper, warning, 10000000.0+4.0);
    limitEq(upper, alarm, 10000000.0+3.0);
#undef limitEq

}

static
void testdbMetaDoubleGet(void)
{
    xRecord* prec = (xRecord*)testdbRecordPtr("recmeta");
    dbChannel *chan = dbChannelCreate("recmeta.OTST");
    db_field_log *pfl;
    evSubscrip evsub;
    long status;
    dbMetaDouble meta;

    STATIC_ASSERT(sizeof(meta.amsg)==sizeof(prec->amsg));
    STATIC_ASSERT(sizeof(meta.amsg)==sizeof(pfl->amsg));

    if(!chan)
        testAbort("Missing recmeta OTST");
    if((status=dbChannelOpen(chan))!=0)
        testAbort("can't open recmeta OTST : %ld", status);

    dbScanLock((dbCommon*)prec);
    /* ensure that all meta-data has different non-zero values */
    prec->otst = 10000000.0;
    prec->sevr = INVALID_ALARM;
    prec->stat = UDF_ALARM;
    strcpy(prec->amsg, "oops");
    prec->acks = MAJOR_ALARM;
    prec->time.secPastEpoch = 0x12345678;
    prec->time.nsec = 0x90abcdef;
    prec->utag = 0x10203040;

    testDiag("dbGet directly from record");
    checkDoubleGet(chan, NULL);

    testDiag("dbGet from field log");

    /* bare minimum init for db_create_event_log() */
    memset(&evsub, 0, sizeof(evsub));
    evsub.chan = chan;
    evsub.useValque = 1;
    pfl = db_create_event_log(&evsub);
    /* spoil things which should now come from field log */
    prec->sevr = 0;
    prec->stat = 0;
    strcpy(prec->amsg, "invalid");
    prec->time.secPastEpoch = 0xdeadbeef;
    prec->time.nsec = 0xdeadbeef;
    prec->utag = 0xdeadbeef;

    /* never any filters, so skip pre/post */
    checkDoubleGet(chan, pfl);
    db_delete_field_log(pfl);

    dbScanUnlock((dbCommon*)prec);
}

typedef struct {
    DBRstatus
    DBRamsg
    DBRtime
    DBRutag
    DBRenumStrs
} dbMetaEnum;

static
void testdbMetaEnumSizes(void)
{
    dbMetaEnum meta;
    size_t pos=0;

    testDiag("dbMetaEnum may not have implicit padding");
#define testOffset(FLD) do {\
    testOk(offsetof(dbMetaEnum, FLD)==pos, "offset(meta, " #FLD "), %u == %u", (unsigned)offsetof(dbMetaEnum, FLD), (unsigned)pos); \
    pos += sizeof(meta.FLD); \
}while(0)

    testOffset(status);
    testOffset(severity);
    testOffset(acks);
    testOffset(ackt);
    testOffset(amsg);
    testOffset(time);
    testOffset(utag);
    testOffset(no_str);
    testOffset(padenumStrs);
    testOffset(strs);
#undef testOffset
    testOk(sizeof(dbMetaEnum)==pos, "sizeof(dbMetaEnum), %u == %u", (unsigned)sizeof(dbMetaEnum), (unsigned)pos);
}

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

static
void testLongField(void)
{
    testDiag("testLongField()");

    testdbGetFieldEqual("lnktest.NAME", DBR_STRING, "lnktest");
    testdbGetFieldEqual("lnktest.NAME$", DBR_STRING, "108");
    testDiag("dbGet() w/ nRequest==1 gets only trailing nil");
    testdbGetFieldEqual("lnktest.NAME$", DBR_CHAR, '\0');
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 10, 8, "lnktest");

    testDiag("get w/ truncation");
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 0, 0, NULL);
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 1, 1, "");
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 2, 2, "l");
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 3, 3, "ln");
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 7, 7, "lnktes");
    testdbGetArrFieldEqual("lnktest.NAME$", DBR_CHAR, 8, 8, "lnktest");
}

static
void testPutArr(void)
{
    epicsUInt32 buf[10];
    testDiag("testPutArr()");

    testdbGetArrFieldEqual("arr", DBR_LONG, 1, 0, NULL);

    buf[0] = 1; buf[1] = 2; buf[2] = 3; buf[3] = 0;
    testdbPutArrFieldOk("arr", DBF_ULONG, 3, buf);

    buf[3] = 0xdeadbeef;
    testdbGetArrFieldEqual("arr", DBR_LONG, 4, 3, buf);
}

static
void testPutSpecial(void)
{
    const char val[] = "special";
    const char inp[] = "special.INP";
    const char sfx[] = "special.SFX";
    testDiag("testPutSpecial()");

    /* There are separate sets of calls to dbPutSpecial() in
     * dbPut() and in dbPutFieldLink() so we need to check
     * both regular fields and link fields.
     */
    testdbPutFieldOk(val, DBR_LONG, 1);
    testdbPutFieldOk(inp, DBR_STRING, "1.0");
    testdbPutFieldOk(sfx, DBR_LONG, SFX_Before);    /* Reject early */
    testdbPutFieldFail(S_db_Blocked, val, DBR_LONG, 2);
    testdbGetFieldEqual(val, DBR_LONG, 1);          /* Wasn't modified */
    testdbPutFieldFail(S_db_Blocked, inp, DBR_STRING, "2.0");
    testdbGetFieldEqual(inp, DBR_STRING, "1.0");    /* Wasn't modified */
    testdbPutFieldOk(sfx, DBR_LONG, SFX_After);     /* Reject late */
    testdbPutFieldFail(S_db_Blocked, val, DBR_LONG, 3);
    testdbPutFieldFail(S_db_Blocked, inp, DBR_STRING, "3.0");
    testdbPutFieldOk(sfx, DBR_LONG, SFX_None);
    testdbPutFieldOk(val, DBR_LONG, 4);
    testdbPutFieldOk(inp, DBR_STRING, "4.0");
}


void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbPutGet)
{
    testPlan(124);
    testdbPrepare();

    testdbMetaDoubleSizes();
    testdbMetaEnumSizes();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbPutGetTest.db", NULL, NULL);

    testGetString();

    testStringMax();

    eltc(0);
    testIocInitOk();
    eltc(1);

    testdbMetaDoubleGet();

    testLongLink();
    testLongAttr();
    testLongField();

    testPutArr();

    testPutSpecial();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
