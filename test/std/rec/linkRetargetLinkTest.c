/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@gmail.com>
 *
 * Test using several stringout records to retarget the link of another record
 */
#define EPICS_DBCA_PRIVATE_API

#include <string.h>

#include "dbAccess.h"

#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"

#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void testRetarget(void)
{
    testMonitor *lnkmon, *valmon;

    testDiag("In testRetarget");

    lnkmon = testMonitorCreate("rec:ai.INP", DBE_VALUE, 0);
    valmon = testMonitorCreate("rec:ai", DBE_VALUE, 0);

    /* initially rec:ai.INP is CONSTANT */

    testdbGetFieldEqual("rec:ai", DBR_DOUBLE, 0.0);
    testdbGetFieldEqual("rec:ai.INP", DBR_STRING, "0");

    /* rec:ai.INP becomes DB_LINK, but no processing is triggered */
    testdbPutFieldOk("rec:link1.PROC", DBF_LONG, 0);

    testMonitorWait(lnkmon);

    testdbGetFieldEqual("rec:ai", DBR_DOUBLE, 0.0);
    testdbGetFieldEqual("rec:ai.INP", DBR_STRING, "rec:src1 NPP NMS");

    /* trigger a read from rec:ai.INP */
    testdbPutFieldOk("rec:ai.PROC", DBF_LONG, 0);

    testMonitorWait(valmon);

    testdbGetFieldEqual("rec:ai", DBR_DOUBLE, 1.0);

    /* rec:ai.INP becomes CA_LINK w/ CP, processing is triggered */
    testdbPutFieldOk("rec:link2.PROC", DBF_LONG, 0);

    testMonitorWait(lnkmon);
    testMonitorWait(valmon);

    testdbGetFieldEqual("rec:ai", DBR_DOUBLE, 2.0);
    testdbGetFieldEqual("rec:ai.INP", DBR_STRING, "rec:src2 CP NMS");

    testMonitorDestroy(lnkmon);
    testMonitorDestroy(valmon);
}

#define testLongStrEq(PV, VAL) testdbGetArrFieldEqual(PV, DBF_CHAR, sizeof(VAL)+2, sizeof(VAL), VAL)
#define testPutLongStr(PV, VAL) testdbPutArrFieldOk(PV, DBF_CHAR, sizeof(VAL), VAL);

static void testRetargetJLink(void)
{
    testDiag("In testRetargetJLink");

    testdbGetFieldEqual("rec:j1", DBF_DOUBLE, 10.0);
    /* minimal args */
    testLongStrEq("rec:j1.INP$", "{\"calc\":{\"expr\":\"A+5\",\"args\":5}}");

    /* with [] */
    testPutLongStr("rec:j1.INP$", "{\"calc\":{\"expr\":\"A+5\",\"args\":[7]}}");
    testdbPutFieldOk("rec:j1.PROC", DBF_LONG, 1);

    /* with const */
    testPutLongStr("rec:j1.INP$", "{\"calc\":{\"expr\":\"A+5\",\"args\":[{\"const\":7}]}}");
    testdbPutFieldOk("rec:j1.PROC", DBF_LONG, 1);

    testdbGetFieldEqual("rec:j1", DBF_DOUBLE, 12.0);
    testLongStrEq("rec:j1.INP$", "{\"calc\":{\"expr\":\"A+5\",\"args\":[{\"const\":7}]}}");
}

MAIN(linkRetargetLinkTest)
{
    testPlan(18);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("linkRetargetLink.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
    /* wait for local CA links to be connected or dbPutField() will fail */
    /* wait for initial CA_CONNECT actions to be processed.
     * Assume that local CA links deliver callbacks synchronously
     * eg. that ca_create_channel() will invoke the connection callback
     *     before returning.
     */
    dbCaSync();

    testRetarget();
    testRetargetJLink();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
