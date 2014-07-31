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
    const char *linkstring;
    short linkType;
    unsigned int pvlMask;
    const char *linkback;
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

static void testSet(void)
{
    const struct testDataT *td = testSetData;
    xRecord *prec;
    DBLINK *plink;
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

MAIN(dbPutLinkTest)
{
    testPlan(56);
    testSet();
    return testDone();
}
