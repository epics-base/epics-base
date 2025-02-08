/*************************************************************************\
* Copyright (c) 2025 Dirk Zimoch
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Compile and link test for epicsExport.h
* 
*/

#include <epicsUnitTest.h>
#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <iocsh.h>
#include <epicsExport.h>

#ifdef __cplusplus
extern "C"
#endif
int epicsExportTestIoc_registerRecordDeviceDriver(struct dbBase *);

static int* testVarEquals(const char* name, int expected)
{
    const iocshVarDef *var;
    int *p;

    var = iocshFindVariable(name);
    if (!var) {
        testFail("Cannot access variable %s", name);
        return NULL;
    }
    if (var->type != iocshArgInt) {
        testFail("Variable %s has wrong type", name);
        return NULL;
    }
    p = (int*)(var->pval);
    testOk(*p == expected, "Variable %s == %d", name, expected);
    return p;
}

MAIN(epicsExportTest)
{
    int *p1, *p2;

    testPlan(31);
    testdbPrepare();
    testdbReadDatabase("epicsExportTestIoc.dbd", 0, 0);
    testOk(epicsExportTestIoc_registerRecordDeviceDriver(pdbbase)==0, "registerRecordDeviceDriver");

    testDiag("Testing if dsets and functions are found");
    testdbReadDatabase("epicsExportTest.db", 0, 0);
    testIocInitOk();

    testDiag("Testing if dsets work correctly");
    testdbGetFieldEqual("li1", DBF_LONG, -1);
    testdbGetFieldEqual("li2", DBF_LONG, -2);
    testdbPutFieldOk("li1.PROC", DBF_LONG, 1);
    testdbPutFieldOk("li2.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("li1", DBF_LONG, 5);
    testdbGetFieldEqual("li2", DBF_LONG, 5);

    testDiag("Testing if functions work correctly");
    testdbGetFieldEqual("asub1.A", DBF_LONG, 1);
    testdbGetFieldEqual("asub1.B", DBF_LONG, -2);
    testdbGetFieldEqual("asub2.A", DBF_LONG, 3);
    testdbGetFieldEqual("asub2.B", DBF_LONG, -4);
    testdbPutFieldOk("asub1.PROC", DBF_LONG, 1);
    testdbPutFieldOk("asub2.PROC", DBF_LONG, 1);
    testdbGetFieldEqual("asub1.A", DBF_LONG, 1);
    testdbGetFieldEqual("asub1.B", DBF_LONG, 2);
    testdbGetFieldEqual("asub2.A", DBF_LONG, 3);
    testdbGetFieldEqual("asub2.B", DBF_LONG, 4);

    testDiag("Testing if variable access works");
    p1 = testVarEquals("i1", 2);
    p2 = testVarEquals("i2", 2);
    testVarEquals("v1", 10);
    testVarEquals("v2", 20);
    testVarEquals("c1", 100);
    testVarEquals("c2", 200);

    if (p1 && p2) {
        testDiag("Testing if variables are accessible from iocsh");
        testOk(iocshCmd("var i1,4") == 0, "Setting i1 = 4 in iocsh");
        testOk(iocshCmd("var i2,5") == 0, "Setting i2 = 5 in iocsh");
        testOk(*p1==4, "Variable i1 == 4");
        testOk(*p2==5, "Variable i2 == 5");
    }

    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
