/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include <stdlib.h>

#include "epicsSpin.h"
#include "epicsMutex.h"
#include "dbCommon.h"
#include "epicsThread.h"

#include "dbLockPvt.h"
#include "dbStaticLib.h"

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

    actual = rA->lset->plockSet==rB->lset->plockSet;

    testOk(match==actual, "dbLockGetLockId(\"%s\")%c=dbLockGetLockId(\"%s\")",
           A, match?'=':'!', B);
}

#define testIntOk1(A, OP, B) testOk((A) OP (B), "%s (%d) %s %s (%d)", #A, A, #OP, #B, B);
#define testPtrOk1(A, OP, B) testOk((A) OP (B), "%s (%p) %s %s (%p)", #A, A, #OP, #B, B);

static
void testSets(void) {
    DBENTRY entry;
    long status;

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Check that all records have initialized lockRecord and lockSet");

    dbInitEntry(pdbbase, &entry);
    for(status = dbFirstRecordType(&entry);
        !status;
        status = dbNextRecordType(&entry)) {
        for(status = dbFirstRecord(&entry);
            !status;
            status = dbNextRecord(&entry)) {
            dbCommon *prec = entry.precnode->precord;
            testOk(prec->lset!=NULL, "%s.LSET != NULL", prec->name);
            if(prec->lset!=NULL)
                testOk(prec->lset->plockSet!=NULL, "%s.LSET.plockSet != NULL", prec->name);
            else
                testSkip(1, "lockRecord missing");
        }
    }
    dbFinishEntry(&entry);

    testDiag("Check initial creation of DB links");

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

    testOk1(testdbRecordPtr("reca")->lset->plockSet->refcount==1);
    testOk1(testdbRecordPtr("recb")->lset->plockSet->refcount==2);
    testOk1(testdbRecordPtr("recd")->lset->plockSet->refcount==3);

    testIocShutdownOk();

    testdbCleanup();
}

static void testSingleLock(void)
{
    dbCommon *prec;
    testDiag("testing dbScanLock()/dbScanUnlock()");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    prec = testdbRecordPtr("reca");
    testOk1(prec->lset->plockSet->refcount==1);
    dbScanLock(prec);
    /* scan lock does not keep a reference to the lockSet */
    testOk1(prec->lset->plockSet->refcount==1);
    dbScanUnlock(prec);

    dbScanLock(prec);
    dbScanLock(prec);
    /* scan lock can be recursive */
    testOk1(prec->lset->plockSet->refcount==1);
    dbScanUnlock(prec);
    dbScanUnlock(prec);

    testIocShutdownOk();

    testdbCleanup();
}

static void testMultiLock(void)
{
    dbCommon *prec[8];
    dbLocker *plockA;
#ifdef LOCKSET_DEBUG
    epicsThreadId myself = epicsThreadGetIdSelf();
#endif

    testDiag("Test multi-locker function (lock everything)");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    prec[0] = testdbRecordPtr("reca");
    prec[1] = testdbRecordPtr("recb");
    prec[2] = testdbRecordPtr("recc");
    prec[3] = NULL;
    prec[4] = testdbRecordPtr("recd");
    prec[5] = testdbRecordPtr("rece");
    prec[6] = testdbRecordPtr("recf");
    prec[7] = testdbRecordPtr("recg");

    testDiag("Test init refcounts");
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,1);
    testIntOk1(testdbRecordPtr("recb")->lset->plockSet->refcount,==,2);
    testIntOk1(testdbRecordPtr("recd")->lset->plockSet->refcount,==,3);
    testIntOk1(testdbRecordPtr("recg")->lset->plockSet->refcount,==,1);

    plockA = dbLockerAlloc(prec, 8, 0);
    if(!plockA)
        testAbort("dbLockerAlloc() failed");


    testDiag("After locker created");
    /* locker takes 7 references, one for each lockRecord. */
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,2);
    testIntOk1(testdbRecordPtr("recb")->lset->plockSet->refcount,==,4);
    testIntOk1(testdbRecordPtr("recd")->lset->plockSet->refcount,==,6);
    testIntOk1(testdbRecordPtr("recg")->lset->plockSet->refcount,==,2);
#ifdef LOCKSET_DEBUG
    testPtrOk1(testdbRecordPtr("reca")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recb")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recd")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recg")->lset->plockSet->owner,==,NULL);
#endif

    dbScanLockMany(plockA);

    testDiag("After locker locked");
    /* locker takes 4 references, one for each lockSet. */
    testIntOk1(ellCount(&plockA->locked),==,4);
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,3);
    testIntOk1(testdbRecordPtr("recb")->lset->plockSet->refcount,==,5);
    testIntOk1(testdbRecordPtr("recd")->lset->plockSet->refcount,==,7);
    testIntOk1(testdbRecordPtr("recg")->lset->plockSet->refcount,==,3);
#ifdef LOCKSET_DEBUG
    testPtrOk1(testdbRecordPtr("reca")->lset->plockSet->owner,==,myself);
    testPtrOk1(testdbRecordPtr("recb")->lset->plockSet->owner,==,myself);
    testPtrOk1(testdbRecordPtr("recd")->lset->plockSet->owner,==,myself);
    testPtrOk1(testdbRecordPtr("recg")->lset->plockSet->owner,==,myself);
#endif

    /* recursive locking of individual records is allowed */
    dbScanLock(prec[0]);
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,3);
    dbScanUnlock(prec[0]);

    /* recursive locking with dbScanLockMany() isn't
     * dbScanLockMany(plockA); <-- would fail
     */

    dbScanUnlockMany(plockA);

    testDiag("After locker unlocked");
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,2);
    testIntOk1(testdbRecordPtr("recb")->lset->plockSet->refcount,==,4);
    testIntOk1(testdbRecordPtr("recd")->lset->plockSet->refcount,==,6);
    testIntOk1(testdbRecordPtr("recg")->lset->plockSet->refcount,==,2);
#ifdef LOCKSET_DEBUG
    testPtrOk1(testdbRecordPtr("reca")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recb")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recd")->lset->plockSet->owner,==,NULL);
    testPtrOk1(testdbRecordPtr("recg")->lset->plockSet->owner,==,NULL);
#endif

    dbLockerFree(plockA);

    testDiag("After locker free'd");
    testIntOk1(testdbRecordPtr("reca")->lset->plockSet->refcount,==,1);
    testIntOk1(testdbRecordPtr("recb")->lset->plockSet->refcount,==,2);
    testIntOk1(testdbRecordPtr("recd")->lset->plockSet->refcount,==,3);
    testIntOk1(testdbRecordPtr("recg")->lset->plockSet->refcount,==,1);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkBreak(void)
{
    dbCommon *precB, *precC;
    testDiag("Test break link");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    precB = testdbRecordPtr("recb");
    precC = testdbRecordPtr("recc");

    testOk1(precB->lset->plockSet==precC->lset->plockSet);
    testOk1(precB->lset->plockSet->refcount==2);

    /* break the link between B and C */
    testdbPutFieldOk("recb.SDIS", DBR_STRING, "");

    testOk1(precB->lset->plockSet!=precC->lset->plockSet);
    testIntOk1(precB->lset->plockSet->refcount, ==, 1);
    testIntOk1(precC->lset->plockSet->refcount, ==, 1);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkMake(void)
{
    dbCommon *precA, *precG;
    lockSet *lA, *lG;
    testDiag("Test make link");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    precA = testdbRecordPtr("reca");
    lA = dbLockGetRef(precA->lset);
    precG = testdbRecordPtr("recg");
    lG = dbLockGetRef(precG->lset);

    testPtrOk1(precA->lset->plockSet, !=, precG->lset->plockSet);
    testIntOk1(precA->lset->plockSet->refcount, ==, 2);
    testIntOk1(precG->lset->plockSet->refcount, ==, 2);

    /* make a link between A and G */
    testdbPutFieldOk("reca.SDIS", DBR_STRING, "recg");

    testPtrOk1(precA->lset->plockSet, ==, precG->lset->plockSet);
    testIntOk1(precA->lset->plockSet->refcount, ==, 3);

    if(precA->lset->plockSet==lG) {
        testIntOk1(lA->refcount, ==, 1);
        testIntOk1(lG->refcount, ==, 3);
    } else {
        testIntOk1(lA->refcount, ==, 3);
        testIntOk1(lG->refcount, ==, 1);
    }
    dbLockDecRef(lG);
    dbLockDecRef(lA);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkChange(void)
{
    dbCommon *precB, *precC, *precG;
    lockSet *lB, *lG;
    testDiag("Test re-target link");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    precB = testdbRecordPtr("recb");
    precC = testdbRecordPtr("recc");
    precG = testdbRecordPtr("recg");

    lB = dbLockGetRef(precB->lset);
    lG = dbLockGetRef(precG->lset);

    testPtrOk1(lB,==,precC->lset->plockSet);
    testPtrOk1(lB,!=,lG);
    testIntOk1(lB->refcount,==,3);
    testIntOk1(lG->refcount,==,2);

    /* break the link between B and C and replace it
     * with a link between B and G
     */
    testdbPutFieldOk("recb.SDIS", DBR_STRING, "recg");

    testPtrOk1(precB->lset->plockSet,==,lB);
    testPtrOk1(precG->lset->plockSet,==,lB);
    testPtrOk1(precC->lset->plockSet,!=,lB);
    testPtrOk1(precC->lset->plockSet,!=,lG);

    testIntOk1(lB->refcount,==,3);
    testIntOk1(lG->refcount,==,1);
    testIntOk1(precC->lset->plockSet->refcount,==,1);

    dbLockDecRef(lB);
    dbLockDecRef(lG);

    testIocShutdownOk();

    testdbCleanup();
}

static void testLinkNOP(void)
{
    dbCommon *precB, *precC;
    testDiag("Test re-target link to the same destination");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbLockTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    precB = testdbRecordPtr("recb");
    precC = testdbRecordPtr("recc");

    testOk1(precB->lset->plockSet==precC->lset->plockSet);
    testOk1(precB->lset->plockSet->refcount==2);

    /* renew link between B and C */
    testdbPutFieldOk("recb.SDIS", DBR_STRING, "recc");

    testOk1(precB->lset->plockSet==precC->lset->plockSet);
    testOk1(precB->lset->plockSet->refcount==2);

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(dbLockTest)
{
#ifdef LOCKSET_DEBUG
    testPlan(100);
#else
    testPlan(88);
#endif
    testSets();
    testSingleLock();
    testMultiLock();
    testLinkBreak();
    testLinkMake();
    testLinkChange();
    testLinkNOP();
    return testDone();
}
