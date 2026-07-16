/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static
void testPrecEqual(const char *pv, long expected){
    DBADDR addr;
    struct {
        DBRprecision
        epicsFloat64 value;
    } buf;
    long options = DBR_PRECISION;
    long nRequest = 1;

    if (dbNameToAddr(pv, &addr) ||
        dbGetField(&addr, DBR_DOUBLE, &buf, &options, &nRequest, NULL)) {
        testFail("Can't read the precision of %s", pv);
        return;
    }
    testOk(buf.precision.dp == expected, "%s precision (%ld) == %ld",
        pv, buf.precision.dp, expected);
}

/*
 * PREC gives the precision of VAL and of the A-L and LA-LL fields.
 * The remaining DOUBLE fields get theirs from recGblGetPrec(), which
 * substitutes 15 for a PREC outside the range it can print.
 * */
static
void testSelPrecision(void){
    testPrecEqual("sel0.VAL", 17);
    testPrecEqual("sel0.A", 17);
    testPrecEqual("sel0.L", 17);
    testPrecEqual("sel0.LA", 17);
    testPrecEqual("sel0.LL", 17);
    testPrecEqual("sel0.HIHI", 15);
}

MAIN(selTest) {
    testPlan(6);

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("selTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testSelPrecision();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
