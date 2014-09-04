/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include "dbLock.h"

#include "dbUnitTest.h"
#include "testMain.h"

#include "dbAccess.h"
#include "errlog.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static
void compareSets(int match, const char *A, const char *B)
{
    int actual;
    dbCommon *rA, *rB;

    rA = testdbRecordPtr(A);
    rB = testdbRecordPtr(B);

    actual = dbLockGetLockId(rA)==dbLockGetLockId(rB);

    testOk(match==actual, "dbLockGetLockId(\"%s\")%c=dbLockGetLockId(\"%s\")",
           A, match?'=':'!', B);
}

static
void testSets(void) {
    testDiag("Check initial creation of DB links");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* reca is by itself */
    compareSets(0, "reca", "recb");
    compareSets(0, "reca", "recc");
    compareSets(0, "reca", "recd");
    compareSets(0, "reca", "rece");
    compareSets(0, "reca", "recf");

    /* recb and recc should be in a lockset */
    compareSets(1, "recb", "recc");
    compareSets(0, "recb", "recd");
    compareSets(0, "recb", "rece");
    compareSets(0, "recb", "recf");

    compareSets(0, "recc", "recd");
    compareSets(0, "recc", "rece");
    compareSets(0, "recc", "recf");

    /* recd, e, and f should be in a lockset */
    compareSets(1, "recd", "rece");
    compareSets(1, "recd", "recf");

    compareSets(1, "rece", "recf");

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(dbLockTest)
{
    testPlan(15);
    testSets();
    return testDone();
}
