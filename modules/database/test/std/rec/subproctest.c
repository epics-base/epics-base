/*************************************************************************\
* Copyright (c) 2022 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* This test covers tests related to invoking subrecords
 */

#include <testMain.h>
#include <dbUnitTest.h>
#include <dbAccess.h>
#include <iocsh.h>
#include "registryFunction.h"
#include <subRecord.h>

static
long subproc(subRecord *prec)
{
    prec->proc = 77;
    return 0;
}

void subproctest_registerRecordDeviceDriver(struct dbBase *);

MAIN(subproctest)
{
    testPlan(2);

    testdbPrepare();

    testdbReadDatabase("subproctest.dbd", NULL, NULL);
    subproctest_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("subproc", (REGISTRYFUNCTION) subproc);
    testdbReadDatabase("subproctest.db", NULL, "TPRO=0");

    testIocInitOk();
    testDiag("===== Test that invalid link in INPA field fails a put request ======");

    testdbPutFieldFail(-1, "InvalidINPARec.PROC", DBF_LONG, 1);

    /* Since the put to PROC above fails, subproc() never runs
     * and the value of PROC will not be set by subproc().  However,
     * the testdbPutField call above goes through, so we get a partial
     * result of the PROC field being left as 1. */
    testdbGetFieldEqual("InvalidINPARec.PROC", DBF_LONG, 1);
    
    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
