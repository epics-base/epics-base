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
#include "link.h"
#include "dbAccess.h"
#include "registry.h"
#include "dbStaticLib.h"
#include "osiFileName.h"
#include "dbmf.h"
#include "errlog.h"

#include "xRecord.h"

#include "testMain.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static const struct testDataT {
    const char * const linkstring;
    short linkType;
    unsigned int pvlMask;
    const char * const linkback;
} testSetData[] = {
    {"", CONSTANT, 0},
    {"0", CONSTANT, 0},
    {"42", CONSTANT, 0},

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
    testDiag("DB/CA link retargeting");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    prec = (xRecord*)testdbRecordPtr("x1");
    plink = &prec->lnk;

    for(;td->linkstring;td++) {
        testDiag("x1.LNK <- \"%s\"", td->linkstring);

        testdbPutFieldOk("x1.LNK", DBF_STRING, td->linkstring);
        if(td->linkback)
            testdbGetFieldEqual("x1.LNK", DBF_STRING, td->linkback);
        else
            testdbGetFieldEqual("x1.LNK", DBF_STRING, td->linkstring);
        testOk1(plink->type==td->linkType);

        if(plink->type==td->linkType) {
            switch(td->linkType) {
            case CONSTANT:
                if(plink->value.constantStr)
                    testOk1(strcmp(plink->value.constantStr,td->linkstring)==0);
                else if(td->linkstring[0]=='\0')
                    testPass("Empty String");
                else
                    testFail("oops");
                break;

            case DB_LINK:
            case CA_LINK:
                testOk(plink->value.pv_link.pvlMask==td->pvlMask,
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
    case VME_IO:
        testOk1(plink->value.vmeio.card==td->vals[0]);
        testOk1(plink->value.vmeio.signal==td->vals[1]);
        testOk1(strcmp(plink->value.vmeio.parm, td->parm)==0);
        break;
    case CAMAC_IO:
        testOk1(plink->value.camacio.b==td->vals[0]);
        testOk1(plink->value.camacio.c==td->vals[1]);
        testOk1(plink->value.camacio.n==td->vals[2]);
        testOk1(plink->value.camacio.a==td->vals[3]);
        testOk1(plink->value.camacio.f==td->vals[4]);
        testOk1(strcmp(plink->value.camacio.parm, td->parm)==0);
        break;
    case AB_IO:
        testOk1(plink->value.abio.link==td->vals[0]);
        testOk1(plink->value.abio.adapter==td->vals[1]);
        testOk1(plink->value.abio.card==td->vals[2]);
        testOk1(plink->value.abio.signal==td->vals[3]);
        testOk1(strcmp(plink->value.abio.parm, td->parm)==0);
        break;
    case GPIB_IO:
        testOk1(plink->value.gpibio.link==td->vals[0]);
        testOk1(plink->value.gpibio.addr==td->vals[1]);
        testOk1(strcmp(plink->value.gpibio.parm, td->parm)==0);
        break;
    case BITBUS_IO:
        testOk1(plink->value.bitbusio.link==td->vals[0]);
        testOk1(plink->value.bitbusio.node==td->vals[1]);
        testOk1(plink->value.bitbusio.port==td->vals[2]);
        testOk1(plink->value.bitbusio.signal==td->vals[3]);
        testOk1(strcmp(plink->value.bitbusio.parm, td->parm)==0);
        break;
    case INST_IO:
        testOk1(strcmp(plink->value.instio.string, td->parm)==0);
        break;
    case BBGPIB_IO:
        testOk1(plink->value.bbgpibio.link==td->vals[0]);
        testOk1(plink->value.bbgpibio.bbaddr==td->vals[1]);
        testOk1(plink->value.bbgpibio.gpibaddr==td->vals[2]);
        testOk1(strcmp(plink->value.bbgpibio.parm, td->parm)==0);
        break;
    case RF_IO:
        testOk1(plink->value.rfio.cryo==td->vals[0]);
        testOk1(plink->value.rfio.micro==td->vals[1]);
        testOk1(plink->value.rfio.dataset==td->vals[2]);
        testOk1(plink->value.rfio.element==td->vals[3]);
        break;
    case VXI_IO:
        if(plink->value.vxiio.flag==VXIDYNAMIC) {
            testOk1(plink->value.vxiio.frame==td->vals[0]);
            testOk1(plink->value.vxiio.slot==td->vals[1]);
            testOk1(plink->value.vxiio.signal==td->vals[2]);
        } else {
            testOk1(plink->value.vxiio.la==td->vals[0]);
            testOk1(plink->value.vxiio.signal==td->vals[1]);
        }
        testOk1(strcmp(plink->value.vxiio.parm, td->parm)==0);
        break;
    }
}

static void testHWInitSet(void)
{
    const testHWDataT *td = testHWData;
    testDiag("HW link parsing during initialization");
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
        testDiag("%s == \"%s\"", td->recname, td->wval);

        prec = (xRecord*)testdbRecordPtr(td->recname);
        plink = &prec->inp;

        strcpy(buf, td->recname);
        strcat(buf, ".INP");

        testdbGetFieldEqual(buf, DBR_STRING, td->wval);

        testOk(plink->type==td->ltype, "link type %d == %d",
               plink->type, td->ltype);
        if(plink->type==td->ltype) {
            testLink(plink, td);
        }

    }

    testIocShutdownOk();

    testdbCleanup();
}

static const testHWDataT testHWData2[] = {
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
    testDiag("HW link parsing during retarget");
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

        testOk(plink->type==td->ltype, "link type %d == %d",
               plink->type, td->ltype);
        if(plink->type==td->ltype) {
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
    testDiag("Link parsing failures during initialization");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);


    /* this load will fail */
    eltc(0);
    testOk1(dbReadDatabase(&pdbbase, "dbBadLink.db", "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
                           "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL)!=0);

    testIocInitOk();
    eltc(1);

    testdbGetFieldEqual("eVME_IO1.INP", DBR_STRING, "#C0 S0 @");

    prec = (xRecord*)testdbRecordPtr("eVME_IO1");
    plink = &prec->inp;
    testOk1(plink->type==VME_IO);
    testOk1(plink->value.vmeio.parm!=NULL);

    testdbGetFieldEqual("eVME_IO2.INP", DBR_STRING, "#C200 S0 @");

    prec = (xRecord*)testdbRecordPtr("eVME_IO2");
    plink = &prec->inp;
    testOk1(plink->type==VME_IO);
    testOk1(plink->value.vmeio.parm!=NULL);

    testdbGetFieldEqual("eINST_IO.INP", DBR_STRING, "@");

    prec = (xRecord*)testdbRecordPtr("eINST_IO");
    plink = &prec->inp;
    testOk1(plink->type==INST_IO);
    testOk1(plink->value.instio.string!=NULL);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkFail(void)
{
    testDiag("Link parsing failures");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbPutLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* INST_IO doesn't accept empty string */
    testdbPutFieldFail(S_dbLib_badField, "rINST_IO.INP", DBR_STRING, "");

    /* INST_IO accepts invalid input as empty string */
    testdbPutFieldOk("rINST_IO.INP", DBR_STRING, "abc");
    testdbGetFieldEqual("rINST_IO.INP", DBR_STRING, "@");

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

MAIN(dbPutLinkTest)
{
    testPlan(200);
    testCADBSet();
    testHWInitSet();
    testHWMod();
    testLinkInitFail();
    testLinkFail();
    return testDone();
}
