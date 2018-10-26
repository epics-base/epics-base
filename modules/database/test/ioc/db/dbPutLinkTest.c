/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include "string.h"

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "iocInit.h"
#include "dbBase.h"
#include "dbDefs.h"
#include "link.h"
#include "dbAccess.h"
#include "registry.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "osiFileName.h"
#include "dbmf.h"
#include "errlog.h"
#include <epicsAtomic.h>

#include "xRecord.h"
#include "jlinkz.h"

#include "testMain.h"

static
int testStrcmp(int expect, const char *A, const char *B) {
    static const char op[] = "<=>";
    int ret = strcmp(A,B);
    testOk(ret==expect, "\"%s\" %c= \"%s\"",
           A, op[expect+1], B);
    return ret==expect;
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

#define TEST_CONSTANT(SET, EXPECT) {SET, {CONSTANT, EXPECT}}
#define TEST_PV_LINK(SET, PV, MOD) {SET, {PV_LINK, PV, MOD}}

static const struct testParseDataT {
    const char * const str;
    dbLinkInfo info;
} testParseData[] = {
    TEST_CONSTANT("", ""),
    TEST_CONSTANT("0.1", "0.1"),
    TEST_CONSTANT("  0.2\t ", "0.2"),

    TEST_PV_LINK("0.1a", "0.1a", 0),
    TEST_PV_LINK(" hello ", "hello", 0),
    TEST_PV_LINK(" hellox MSI", "hellox", pvlOptMSI),
    TEST_PV_LINK(" world MSICP", "world", pvlOptMSI|pvlOptCP),

    {"#C14 S145 @testing", {VME_IO, "testing", 0, "CS", {14, 145}}},
    {"#C14 S145", {VME_IO, "", 0, "CS", {14, 145}}},
    {"#B11 C12 N13 A14 F15 @cparam", {CAMAC_IO, "cparam", 0, "BCNAF", {11, 12, 13, 14, 15}}},
    {"#B11 C12 N13 A14 F15", {CAMAC_IO, "", 0, "BCNAF", {11, 12, 13, 14, 15}}},
    {" #B111 C112 N113 @cparam", {CAMAC_IO, "cparam", 0, "BCN", {111, 112, 113}}},
    {" @hello world ", {INST_IO, "hello world", 0, "", /*{}*/}},
    {" {\"x\":true} ", {JSON_LINK, "{\"x\":true}", 0, "", /*{}*/}},
    {NULL}
};

static void testLinkParse(void)
{
    const struct testParseDataT *td = testParseData;
    dbLinkInfo info;
    testDiag("\n# Checking link parsing\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    for (;td->str; td++) {
        int i, N;
        testDiag("Parsing \"%s\"", td->str);
        testOk(dbParseLink(td->str, DBF_INLINK, &info) == 0, "Parser returned OK");
        if (!testOk(info.ltype == td->info.ltype, "Link type value"))
            testDiag("Expected %d, got %d", td->info.ltype, info.ltype);
        if (td->info.target && info.target)
            testStrcmp(0, info.target, td->info.target);
        else if(!!td->info.target ^ !!info.target)
            testFail("info target NULL mis-match %s %s", info.target, td->info.target);
        else
            testPass("info target NULL as expected");
        if (info.ltype == td->info.ltype) {
            switch (info.ltype) {
            case PV_LINK:
                if (!testOk(info.modifiers == td->info.modifiers,
                        "PV Link modifier flags"))
                    testDiag("Expected %d, got %d", td->info.modifiers,
                        info.modifiers);
                break;
            case VME_IO:
            case CAMAC_IO:
                testStrcmp(0, info.hwid, td->info.hwid);
                N = strlen(td->info.hwid);
                for (i=0; i<N; i++)
                    testOk(info.hwnums[i]==td->info.hwnums[i], "%d == %d",
                           info.hwnums[i], td->info.hwnums[i]);
            }
        }
        dbFreeLinkInfo(&info);
    }

    testIocShutdownOk();

    testdbCleanup();
}

static const char *testParseFailData[] = {
    "#",
    "#S",
    "#ABC",
    "#A0 B",
    "#A0 B @",
    "#A0 B C @",
    "#R1 M2 D3 E4 @oops", /* RF_IO has no parm */
    NULL
};

static void testLinkFailParse(void)
{
    const char * const *td = testParseFailData;
    dbLinkInfo info;
    testDiag("\n# Check parsing of invalid inputs\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    for(;*td; td++) {
        testOk(dbParseLink(*td, DBF_INLINK, &info) == S_dbLib_badField,
            "dbParseLink correctly rejected \"%s\"", *td);
    }

    testIocShutdownOk();

    testdbCleanup();
}

static const struct testDataT {
    const char * const linkstring;
    short linkType;
    unsigned int pvlMask;
    const char * const linkback;
} testSetData[] = {
    {"", CONSTANT, 0},
    {"0", CONSTANT, 0},
    {"42", CONSTANT, 0},
    {"0x1", CONSTANT, 0},

    {"x1", DB_LINK, 0, "x1 NPP NMS"},
    {"x1.VAL", DB_LINK, 0, "x1.VAL NPP NMS"},
    {"x1.TIME", DB_LINK, 0, "x1.TIME NPP NMS"},
    {"x1 PP", DB_LINK, pvlOptPP, "x1 PP NMS"},
    {"x1 PP MSS", DB_LINK, pvlOptPP|pvlOptMSS, "x1 PP MSS"},
    {"x1 PPMSS", DB_LINK, pvlOptPP|pvlOptMSS, "x1 PP MSS"},
    {"x1 PPMSI", DB_LINK, pvlOptPP|pvlOptMSI, "x1 PP MSI"},

    {"qq", CA_LINK, pvlOptInpNative, "qq NPP NMS"},
    {"qq MSI", CA_LINK, pvlOptInpNative|pvlOptMSI, "qq NPP MSI"},
    {"qq MSICA", CA_LINK, pvlOptInpNative|pvlOptCA|pvlOptMSI, "qq CA MSI"},

    {"x1 CA", CA_LINK, pvlOptInpNative|pvlOptCA, "x1 CA NMS"},
    {NULL}
};

static void testCADBSet(void)
{
    const struct testDataT *td = testSetData;
    xRecord *prec;
    DBLINK *plink;
    testDiag("\n# Checking DB/CA link retargeting\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    prec = (xRecord*)testdbRecordPtr("x1");
    plink = &prec->lnk;

    for (;td->linkstring;td++) {
        testDiag("Trying field value \"%s\"", td->linkstring);

        testdbPutFieldOk("x1.LNK", DBF_STRING, td->linkstring);
        if (td->linkback)
            testdbGetFieldEqual("x1.LNK", DBF_STRING, td->linkback);
        else
            testdbGetFieldEqual("x1.LNK", DBF_STRING, td->linkstring);
        if (!testOk(plink->type == td->linkType, "Link type"))
            testDiag("Expected %d, got %d", td->linkType, plink->type);

        if (plink->type == td->linkType) {
            switch (td->linkType) {
            case CONSTANT:
                if (plink->value.constantStr)
                    testOk1(strcmp(plink->value.constantStr, td->linkstring) == 0);
                else if (td->linkstring[0]=='\0')
                    testPass("Empty String");
                else
                    testFail("oops");
                break;

            case DB_LINK:
            case CA_LINK:
                testOk(plink->value.pv_link.pvlMask == td->pvlMask,
                       "pvlMask %x == %x", plink->value.pv_link.pvlMask, td->pvlMask);
                break;
            }
        }
    }

    testIocShutdownOk();

    testdbCleanup();
}

typedef struct {
    const char * const recname;
    short ltype;
    const char * const wval;
    short vals[5];
    const char * const parm;
} testHWDataT;

static const testHWDataT testHWData[] = {
    {"rJSON_LINK", JSON_LINK, "{\"x\":true}", {0}, "{\"x\":true}"},
    {"rVME_IO", VME_IO, "#C100 S101 @parm VME_IO", {100, 101}, "parm VME_IO"},
    {"rCAMAC_IO", CAMAC_IO, "#B11 C12 N13 A14 F15 @parm CAMAC_IO", {11, 12, 13, 14, 15}, "parm CAMAC_IO"},
    {"rAB_IO", AB_IO, "#L21 A22 C23 S24 @parm AB_IO", {21, 22, 23, 24}, "parm AB_IO"},
    {"rGPIB_IO", GPIB_IO, "#L31 A32 @parm GPIB_IO", {31, 32}, "parm GPIB_IO"},
    {"rBITBUS_IO", BITBUS_IO, "#L41 N42 P43 S44 @parm BITBUS_IO", {41, 42, 43, 44}, "parm BITBUS_IO"},
    {"rINST_IO", INST_IO, "@parm INST_IO", {0}, "parm INST_IO"},
    {"rBBGPIB_IO", BBGPIB_IO, "#L51 B52 G53 @parm BBGPIB_IO", {51, 52, 53}, "parm BBGPIB_IO"},
    {"rRF_IO", RF_IO, "#R61 M62 D63 E64", {61, 62, 63, 64}, NULL},
    {"rVXI_IO1", VXI_IO, "#V71 C72 S73 @parm1 VXI_IO", {71, 72, 73}, "parm1 VXI_IO"},
    {"rVXI_IO2", VXI_IO, "#V81 S82 @parm2 VXI_IO", {81, 82}, "parm2 VXI_IO"},
    {NULL}
};

static void testLink(DBLINK *plink, const testHWDataT *td)
{
    switch(td->ltype) {
    case JSON_LINK:
        testOk1(strcmp(plink->value.json.string, td->parm) == 0);
        break;
    case VME_IO:
        testOk1(plink->value.vmeio.card == td->vals[0]);
        testOk1(plink->value.vmeio.signal == td->vals[1]);
        testOk1(strcmp(plink->value.vmeio.parm, td->parm) == 0);
        break;
    case CAMAC_IO:
        testOk1(plink->value.camacio.b == td->vals[0]);
        testOk1(plink->value.camacio.c == td->vals[1]);
        testOk1(plink->value.camacio.n == td->vals[2]);
        testOk1(plink->value.camacio.a == td->vals[3]);
        testOk1(plink->value.camacio.f == td->vals[4]);
        testOk1(strcmp(plink->value.camacio.parm, td->parm) == 0);
        break;
    case AB_IO:
        testOk1(plink->value.abio.link == td->vals[0]);
        testOk1(plink->value.abio.adapter == td->vals[1]);
        testOk1(plink->value.abio.card == td->vals[2]);
        testOk1(plink->value.abio.signal == td->vals[3]);
        testOk1(strcmp(plink->value.abio.parm, td->parm) == 0);
        break;
    case GPIB_IO:
        testOk1(plink->value.gpibio.link == td->vals[0]);
        testOk1(plink->value.gpibio.addr == td->vals[1]);
        testOk1(strcmp(plink->value.gpibio.parm, td->parm) == 0);
        break;
    case BITBUS_IO:
        testOk1(plink->value.bitbusio.link == td->vals[0]);
        testOk1(plink->value.bitbusio.node == td->vals[1]);
        testOk1(plink->value.bitbusio.port == td->vals[2]);
        testOk1(plink->value.bitbusio.signal == td->vals[3]);
        testOk1(strcmp(plink->value.bitbusio.parm, td->parm) == 0);
        break;
    case INST_IO:
        testOk1(strcmp(plink->value.instio.string, td->parm) == 0);
        break;
    case BBGPIB_IO:
        testOk1(plink->value.bbgpibio.link == td->vals[0]);
        testOk1(plink->value.bbgpibio.bbaddr == td->vals[1]);
        testOk1(plink->value.bbgpibio.gpibaddr == td->vals[2]);
        testOk1(strcmp(plink->value.bbgpibio.parm, td->parm) == 0);
        break;
    case RF_IO:
        testOk1(plink->value.rfio.cryo == td->vals[0]);
        testOk1(plink->value.rfio.micro == td->vals[1]);
        testOk1(plink->value.rfio.dataset == td->vals[2]);
        testOk1(plink->value.rfio.element == td->vals[3]);
        break;
    case VXI_IO:
        if(plink->value.vxiio.flag == VXIDYNAMIC) {
            testOk1(plink->value.vxiio.frame == td->vals[0]);
            testOk1(plink->value.vxiio.slot == td->vals[1]);
            testOk1(plink->value.vxiio.signal == td->vals[2]);
        } else {
            testOk1(plink->value.vxiio.la == td->vals[0]);
            testOk1(plink->value.vxiio.signal == td->vals[1]);
        }
        testOk1(strcmp(plink->value.vxiio.parm, td->parm) == 0);
        break;
    }
}

static void testHWInitSet(void)
{
    const testHWDataT *td = testHWData;
    testDiag("\n# Checking HW link parsing during initialization\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    for (;td->recname; td++) {
        char buf[MAX_STRING_SIZE];
        xRecord *prec;
        DBLINK *plink;

        testDiag("%s == \"%s\"", td->recname, td->wval);

        prec = (xRecord *) testdbRecordPtr(td->recname);
        plink = &prec->inp;

        strcpy(buf, td->recname);
        strcat(buf, ".INP");

        testdbGetFieldEqual(buf, DBR_STRING, td->wval);

        if (!testOk(plink->type == td->ltype, "Link type")) {
            testDiag("Expected %d, got %d",
               td->ltype, plink->type);
        }
        else {
            testLink(plink, td);
        }

    }

    testIocShutdownOk();

    testdbCleanup();
}

static const testHWDataT testHWData2[] = {
    {"rJSON_LINK", JSON_LINK, "{\"x\":true}", {0}, "{\"x\":true}"},
    {"rVME_IO", VME_IO, "#C200 S201 @another VME_IO", {200, 201}, "another VME_IO"},
    {"rCAMAC_IO", CAMAC_IO, "#B111 C112 N113 A114 F115 @CAMAC_IO", {111, 112, 113, 114, 115}, "CAMAC_IO"},
    {"rAB_IO", AB_IO, "#L121 A122 C123 S124 @another AB_IO", {121, 122, 123, 124}, "another AB_IO"},
    {"rGPIB_IO", GPIB_IO, "#L131 A132 @another GPIB_IO", {131, 132}, "another GPIB_IO"},
    {"rBITBUS_IO", BITBUS_IO, "#L141 N142 P143 S144 @BITBUS_IO", {141, 142, 143, 144}, "BITBUS_IO"},
    {"rINST_IO", INST_IO, "@another INST_IO", {0}, "another INST_IO"},
    {"rBBGPIB_IO", BBGPIB_IO, "#L151 B152 G153 @another BBGPIB_IO", {151, 152, 153}, "another BBGPIB_IO"},
    {"rRF_IO", RF_IO, "#R161 M162 D163 E164", {161, 162, 163, 164}, NULL},
    {"rVXI_IO1", VXI_IO, "#V171 C172 S173 @another1 VXI_IO", {171, 172, 173}, "another1 VXI_IO"},
    {"rVXI_IO2", VXI_IO, "#V181 S182 @another2 VXI_IO", {181, 182}, "another2 VXI_IO"},
    {NULL}
};

static void testHWMod(void)
{
    const testHWDataT *td = testHWData2;

    testDiag("\n# Checking HW link parsing during retarget\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    for(;td->recname;td++) {
        char buf[MAX_STRING_SIZE];
        xRecord *prec;
        DBLINK *plink;
        testDiag("%s -> \"%s\"", td->recname, td->wval);

        prec = (xRecord*)testdbRecordPtr(td->recname);
        plink = &prec->inp;

        strcpy(buf, td->recname);
        strcat(buf, ".INP");

        testdbPutFieldOk(buf, DBR_STRING, td->wval);

        testdbGetFieldEqual(buf, DBR_STRING, td->wval);

        if (!testOk(plink->type == td->ltype, "Link type")) {
            testDiag("Expected %d, got %d",
               td->ltype, plink->type);
        }
        else {
            testLink(plink, td);
        }

    }

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkInitFail(void)
{
    xRecord *prec;
    DBLINK *plink;
    testDiag("\n# Checking link parse failures at iocInit\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    /* this load will fail */
    eltc(0);
    testOk(dbReadDatabase(&pdbbase, "dbBadLink.db", "." OSI_PATH_LIST_SEPARATOR
        ".." OSI_PATH_LIST_SEPARATOR "../O.Common" OSI_PATH_LIST_SEPARATOR
	"O.Common", NULL) != 0, "dbReadDatabase returned error (expected)");

    testIocInitOk();
    eltc(1);

    testdbGetFieldEqual("eVME_IO1.INP", DBR_STRING, "#C0 S0 @");

    prec = (xRecord *) testdbRecordPtr("eVME_IO1");
    plink = &prec->inp;
    testOk1(plink->type == VME_IO);
    testOk1(plink->value.vmeio.parm != NULL);

    testdbGetFieldEqual("eVME_IO2.INP", DBR_STRING, "#C0 S0 @");

    prec = (xRecord *) testdbRecordPtr("eVME_IO2");
    plink = &prec->inp;
    testOk1(plink->type == VME_IO);
    testOk1(plink->value.vmeio.parm != NULL);

    testdbGetFieldEqual("eINST_IO.INP", DBR_STRING, "@");

    prec = (xRecord *) testdbRecordPtr("eINST_IO");
    plink = &prec->inp;
    testOk1(plink->type == INST_IO);
    testOk1(plink->value.instio.string != NULL);

    testdbGetFieldEqual("eINST_IO2.INP", DBR_STRING, "@");

    prec = (xRecord *) testdbRecordPtr("eINST_IO2");
    plink = &prec->inp;
    testOk1(plink->type == INST_IO);
    testOk1(plink->value.instio.string != NULL);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkFail(void)
{
    testDiag("\n# Checking runtime link parse failures\n#");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* INST_IO doesn't accept empty string */
    testdbPutFieldFail(S_dbLib_badField, "rINST_IO.INP", DBR_STRING, "");

    /* INST_IO doesn't accept string without @ */
    testdbPutFieldFail(S_dbLib_badField, "rINST_IO.INP", DBR_STRING, "abc");

    /* JSON_LINK dies when expected */
    testdbPutFieldOk("rJSON_LINK.INP", DBR_STRING, "{\"x\":true}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":false}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":null}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":1}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":1.1}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":\"x\"}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":[]}");
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":{}}");

    /* JSON_LINK syntax errors */
    testdbPutFieldFail(S_dbLib_badField, "rJSON_LINK.INP", DBR_STRING, "{\"x\":bbbb}");

    /* syntax errors */
    testdbPutFieldFail(S_dbLib_badField, "rVME_IO.INP", DBR_STRING, "#S201 C200 @another VME_IO");
    testdbPutFieldFail(S_dbLib_badField, "rVME_IO.INP", DBR_STRING, "C200 #S201");

    /* VME_IO doesn't accept empty string */
    testdbPutFieldFail(S_dbLib_badField, "rVME_IO.INP", DBR_STRING, "");

    testdbPutFieldOk("rVME_IO.INP", DBR_STRING, "#C1 S2 @hello");

    /* VME_IO fails invalid input */
    testdbPutFieldFail(S_dbLib_badField, "rVME_IO.INP", DBR_STRING, "abc");

    testIocShutdownOk();

    testdbCleanup();
}

static
void testNumZ(int expect)
{
    int numz = epicsAtomicGetIntT(&numzalloc);
    testOk(numz==expect, "numzalloc==%d (%d)", expect, numz);
}

static
void testJLink(void)
{
    testDiag("Test json link setup/retarget");

    testNumZ(0);

    testDiag("Link parsing failures");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);
    testdbReadDatabase("dbPutLinkTestJ.db", NULL, NULL);

    testNumZ(0);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testNumZ(6);

    testdbPutFieldOk("j1.PROC", DBF_LONG, 1);
    testdbPutFieldOk("j2.PROC", DBF_LONG, 1);
    testdbPutFieldOk("j3.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("j1.INP", DBF_STRING, "{\"z\":{\"good\":1}}");
    testdbGetFieldEqual("j1.VAL", DBF_LONG, 1);
    testdbGetFieldEqual("j2.VAL", DBF_LONG, 2);
    testdbGetFieldEqual("j3.VAL", DBF_LONG, 3);

    testNumZ(6);

    testdbPutFieldOk("j1.INP", DBF_STRING, "{\"z\":{\"good\":4}}");
    testdbPutFieldOk("j1.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("j1.VAL", DBF_LONG, 4);

    testdbPutFieldOk("j2.TSEL", DBF_STRING, "{\"z\":{\"good\":0}}");
    testdbPutFieldOk("j2.PROC", DBF_LONG, 1);

    testNumZ(7);

    testdbPutFieldFail(S_dbLib_badField, "j1.INP", DBF_STRING, "{\"z\":{\"fail\":5}}");
    testdbPutFieldFail(S_dbLib_badField, "j1.INP", DBF_STRING, "{\"z\":{\"good\":6}");
    testdbPutFieldOk("j1.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("j1.VAL", DBF_LONG, 4);
    /* put failures in parsing stage don't modify link */
    testdbGetFieldEqual("j1.INP", DBF_STRING, "{\"z\":{\"good\":4}}");

    testNumZ(7);

    /* Check SDIS using a JSON link prevents processing */
    testdbPutFieldOk("j1.SDIS", DBF_STRING, "{\"z\":{\"good\":1}}");
    testdbPutFieldOk("j1.INP", DBF_STRING, "{\"z\":{\"good\":1}}");
    testdbPutFieldOk("j1.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("j1.VAL", DBF_LONG, 4);

    testIocShutdownOk();

    testNumZ(0);

    testdbCleanup();
}

static
void testTSEL(void)
{
    dbCommon *rec[2];
    dbLocker *locker;

    testDiag("Test TSEL link to .TIME");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    rec[0] = testdbRecordPtr("time:one");
    rec[1] = testdbRecordPtr("time:two");

    eltc(0);
    testIocInitOk();
    eltc(1);

    locker = dbLockerAlloc(rec, NELEMENTS(rec), 0);
    if(!locker)
        testAbort("dbLockerAlloc() fails");

    testdbPutFieldOk("time:one.PROC", DBF_LONG, 1);

    /* wait a bit so that we would get different timestamps */
    epicsThreadSleep(0.001);

    testdbPutFieldOk("time:two.PROC", DBF_LONG, 1);

#define COMPARE(MSG, C, TS1, TS2) testOk((C)^((TS1)->secPastEpoch == (TS2)->secPastEpoch && (TS1)->nsec == (TS2)->nsec), \
    MSG " %u:%u == %u:%u", (unsigned)(TS1)->secPastEpoch, (unsigned)(TS1)->nsec, \
            (unsigned)(TS2)->secPastEpoch, (unsigned)(TS2)->nsec)

    testDiag("Check initially connected TSEL link");
    dbScanLockMany(locker);
    COMPARE("first", 0, &rec[0]->time, &rec[1]->time);
    testOk1(rec[1]->tsel.flags & DBLINK_FLAG_TSELisTIME);
    dbScanUnlockMany(locker);

    testdbPutFieldOk("time:two.TSEL", DBF_STRING, "");

    testdbPutFieldOk("time:two.PROC", DBF_LONG, 1);

    testDiag("Check no TSEL link");
    dbScanLockMany(locker);
    COMPARE("second", 1, &rec[0]->time, &rec[1]->time);
    testOk1(!(rec[1]->tsel.flags & DBLINK_FLAG_TSELisTIME));
    dbScanUnlockMany(locker);

    testdbPutFieldOk("time:two.TSEL", DBF_STRING, "time:one.TIME");

    testdbPutFieldOk("time:two.PROC", DBF_LONG, 1);

    testDiag("Check re-connected TSEL link");
    dbScanLockMany(locker);
    COMPARE("third", 0, &rec[0]->time, &rec[1]->time);
    testOk1(rec[1]->tsel.flags & DBLINK_FLAG_TSELisTIME);
    dbScanUnlockMany(locker);

    dbLockerFree(locker);

    testIocShutdownOk();

    testdbCleanup();
#undef COMPARE
}

MAIN(dbPutLinkTest)
{
    testPlan(337);
    testLinkParse();
    testLinkFailParse();
    testCADBSet();
    testHWInitSet();
    testHWMod();
    testLinkInitFail();
    testLinkFail();
    testJLink();
    testTSEL();
    return testDone();
}
