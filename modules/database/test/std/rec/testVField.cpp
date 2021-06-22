/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <dbUnitTest.h>
#include <testMain.h>

#include <dbAccess.h>
#include <stdstringinRecord.h>

extern "C" {
void recTestIoc_registerRecordDeviceDriver(struct dbBase *);
}

namespace {

void testGetPut()
{
    DBADDR addr;
    stdstringinRecord *psrc = (stdstringinRecord*)testdbRecordPtr("recsrc");
    stdstringinRecord *pdst = (stdstringinRecord*)testdbRecordPtr("recdst");

    if(!testOk1(!dbNameToAddr("recsrc", &addr))) {
        testAbort("Unable to create DBADDR to vfield");
    }

    VString sval;
    sval.vtype = &vfStdString;
    sval.value = "This is a long test value to ensure std::string allocates";

    testdbPutFieldOk("recsrc", DBR_VFIELD, &sval);

    dbScanLock((dbCommon*)psrc);
    testOk1(psrc->val==sval.value);
    dbScanUnlock((dbCommon*)psrc);

    VString sval2;
    sval2.vtype = &vfStdString;

    testOk1(!dbGetField(&addr, DBR_VFIELD, &sval2, 0, 0, 0));
    testOk1(sval2.value==sval.value);

    testdbPutFieldOk("recdst.PROC", DBF_LONG, 1);

    dbScanLock((dbCommon*)pdst);
    testOk1(pdst->val==sval.value);
    dbScanUnlock((dbCommon*)pdst);

    testdbGetArrFieldEqual("recsrc", DBF_CHAR, sval.value.size()+1, sval.value.size()+1, sval.value.c_str());
}

}

MAIN(testVField)
{
    testPlan(8);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", 0, 0);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("ssiTest.db", 0, 0);

    testIocInitOk();

    testGetPut();

    testIocShutdownOk();

    testdbCleanup();

    return  testDone();
}
