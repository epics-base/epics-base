/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "dbLock.h"
#include "errlog.h"
#include "dbAccess.h"
#include "epicsMath.h"

#include "aiRecord.h"
#include "compressRecord.h"

#define testDEq(A,B,D) testOk(fabs((A)-(B))<(D), #A " (%f) ~= " #B " (%f)", A, B)

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static
void checkArrD(const char *pv, long elen, double a, double b, double c, double d)
{
    double buf[4];
    double expect[4];
    long nReq = NELEMENTS(buf), i;
    unsigned match;
    DBADDR addr;

    expect[0] = a;
    expect[1] = b;
    expect[2] = c;
    expect[3] = d;

    if (dbNameToAddr(pv, &addr))
        testAbort("Unknown PV '%s'", pv);

    if (dbGet(&addr, DBR_DOUBLE, buf, NULL, &nReq, NULL))
        testAbort("Failed to get '%s'", pv);

    match = elen==nReq;
    for (i=0; i<nReq && i<elen; i++) {
        match &= fabs(buf[i]-expect[i])<0.01;
    }
    testOk(match, "dbGet(\"%s\") matches", pv);
    if (elen!=nReq)
        testDiag("lengths don't match %ld != %ld", elen, nReq);
    for (i=0; i<nReq && i<elen; i++) {
        if (fabs(buf[i]-expect[i])>=0.01)
            testDiag("[%ld] -> %f != %f", i, expect[i], buf[i]);
    }
}

static
void checkArrI(const char *pv, long elen, epicsInt32 a, epicsInt32 b, epicsInt32 c, epicsInt32 d)
{
    epicsInt32 buf[4];
    epicsInt32 expect[4];
    long nReq = NELEMENTS(buf), i;
    unsigned match;
    DBADDR addr;

    expect[0] = a;
    expect[1] = b;
    expect[2] = c;
    expect[3] = d;

    if (dbNameToAddr(pv, &addr))
        testAbort("Unknown PV '%s'", pv);

    if (dbGet(&addr, DBR_LONG, buf, NULL, &nReq, NULL))
        testAbort("Failed to get '%s'", pv);

    match = elen==nReq;
    for (i=0; i<nReq && i<elen; i++) {
        match &= buf[i]==expect[i];
    }
    testOk(match, "dbGet(\"%s\") matches", pv);
    if (elen!=nReq)
        testDiag("lengths don't match %ld != %ld", elen, nReq);
    for (i=0; i<nReq && i<elen; i++) {
        if(buf[i]!=expect[i])
            testDiag("[%ld] -> %d != %d", i, (int)expect[i], (int)buf[i]);
    }
}

static
void testFIFOCirc(void)
{
    aiRecord *vrec;
    compressRecord *crec;
    double *cbuf;

    testDiag("Test FIFO");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("compressTest.db", NULL, "ALG=Circular Buffer,BALG=FIFO Buffer,NSAM=4");

    vrec = (aiRecord*)testdbRecordPtr("val");
    crec = (compressRecord*)testdbRecordPtr("comp");

    eltc(0);
    testIocInitOk();
    eltc(1);

    dbScanLock((dbCommon*)crec);
    cbuf = crec->bptr;

    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==0);

    testDiag("Push 1.1");
    vrec->val = 1.1;
    dbProcess((dbCommon*)crec);

    /* In FIFO mode the valid elements are
     *   cbuf[(off-nuse-1) % size] through cbuf[(off-1) % size]
     */
    testOk1(crec->off==1);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==1);
    testDEq(cbuf[0], 1.1, 0.1);
    testDEq(cbuf[1], 0.0, 0.1);
    testDEq(cbuf[2], 0.0, 0.1);
    testDEq(cbuf[3], 0.0, 0.1);
    checkArrD("comp", 1, 1.1, 0, 0, 0);

    testDiag("Push 2.1");
    vrec->val = 2.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==2);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==2);
    testDEq(cbuf[0], 1.1, 0.1);
    testDEq(cbuf[1], 2.1, 0.1);
    testDEq(cbuf[2], 0.0, 0.1);
    testDEq(cbuf[3], 0.0, 0.1);
    checkArrD("comp", 2, 1.1, 2.1, 0, 0);

    testDiag("Push 3.1");
    vrec->val = 3.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==3);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==3);
    testDEq(cbuf[0], 1.1, 0.1);
    testDEq(cbuf[1], 2.1, 0.1);
    testDEq(cbuf[2], 3.1, 0.1);
    testDEq(cbuf[3], 0.0, 0.1);
    checkArrD("comp", 3, 1.1, 2.1, 3.1, 0);

    testDiag("Push 4.1");
    vrec->val = 4.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 1.1, 0.1);
    testDEq(cbuf[1], 2.1, 0.1);
    testDEq(cbuf[2], 3.1, 0.1);
    testDEq(cbuf[3], 4.1, 0.1);
    checkArrD("comp", 4, 1.1, 2.1, 3.1, 4.1);

    testDiag("Push 5.1");
    vrec->val = 5.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==1);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 5.1, 0.1);
    testDEq(cbuf[1], 2.1, 0.1);
    testDEq(cbuf[2], 3.1, 0.1);
    testDEq(cbuf[3], 4.1, 0.1);
    checkArrD("comp", 4, 2.1, 3.1, 4.1, 5.1);

    testDiag("Push 6.1");
    vrec->val = 6.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==2);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 5.1, 0.1);
    testDEq(cbuf[1], 6.1, 0.1);
    testDEq(cbuf[2], 3.1, 0.1);
    testDEq(cbuf[3], 4.1, 0.1);
    checkArrD("comp", 4, 3.1, 4.1, 5.1, 6.1);

    dbScanUnlock((dbCommon*)crec);

    testDiag("Reset");
    testdbPutFieldOk("comp.RES", DBF_LONG, 0);

    dbScanLock((dbCommon*)crec);
    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==0);
    checkArrD("comp", 0, 0, 0, 0, 0);
    dbScanUnlock((dbCommon*)crec);

    testIocShutdownOk();

    testdbCleanup();
}

static
void testLIFOCirc(void)
{
    aiRecord *vrec;
    compressRecord *crec;
    double *cbuf;

    testDiag("Test LIFO");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("compressTest.db", NULL,
        "ALG=Circular Buffer,BALG=LIFO Buffer,NSAM=4");

    vrec = (aiRecord*)testdbRecordPtr("val");
    crec = (compressRecord*)testdbRecordPtr("comp");

    eltc(0);
    testIocInitOk();
    eltc(1);

    dbScanLock((dbCommon*)crec);
    cbuf = crec->bptr;

    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==0);

    testDiag("Push 1.1");
    vrec->val = 1.1;
    dbProcess((dbCommon*)crec);

    testDiag("off %u", crec->off);
    testOk1(crec->off==3);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==1);
    testDEq(cbuf[0], 0.0, 0.1);
    testDEq(cbuf[1], 0.0, 0.1);
    testDEq(cbuf[2], 0.0, 0.1);
    testDEq(cbuf[3], 1.1, 0.1);
    checkArrD("comp", 1, 1.1, 0, 0, 0);

    testDiag("Push 2.1");
    vrec->val = 2.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==2);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==2);
    testDEq(cbuf[0], 0.0, 0.1);
    testDEq(cbuf[1], 0.0, 0.1);
    testDEq(cbuf[2], 2.1, 0.1);
    testDEq(cbuf[3], 1.1, 0.1);
    checkArrD("comp", 2, 2.1, 1.1, 0, 0);
    checkArrI("comp", 2, 2, 1, 0, 0);

    testDiag("Push 3.1");
    vrec->val = 3.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==1);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==3);
    testDEq(cbuf[0], 0.0, 0.1);
    testDEq(cbuf[1], 3.1, 0.1);
    testDEq(cbuf[2], 2.1, 0.1);
    testDEq(cbuf[3], 1.1, 0.1);
    checkArrD("comp", 3, 3.1, 2.1, 1.1, 0);
    checkArrI("comp", 3, 3, 2, 1, 0);

    testDiag("Push 4.1");
    vrec->val = 4.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 4.1, 0.1);
    testDEq(cbuf[1], 3.1, 0.1);
    testDEq(cbuf[2], 2.1, 0.1);
    testDEq(cbuf[3], 1.1, 0.1);
    checkArrD("comp", 4, 4.1, 3.1, 2.1, 1.1);
    checkArrI("comp", 4, 4, 3, 2, 1);

    testDiag("Push 5.1");
    vrec->val = 5.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==3);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 4.1, 0.1);
    testDEq(cbuf[1], 3.1, 0.1);
    testDEq(cbuf[2], 2.1, 0.1);
    testDEq(cbuf[3], 5.1, 0.1);
    checkArrD("comp", 4, 5.1, 4.1, 3.1, 2.1);
    checkArrI("comp", 4, 5, 4, 3, 2);

    testDiag("Push 6.1");
    vrec->val = 6.1;
    dbProcess((dbCommon*)crec);

    testOk1(crec->off==2);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==4);
    testDEq(cbuf[0], 4.1, 0.1);
    testDEq(cbuf[1], 3.1, 0.1);
    testDEq(cbuf[2], 6.1, 0.1);
    testDEq(cbuf[3], 5.1, 0.1);
    checkArrD("comp", 4, 6.1, 5.1, 4.1, 3.1);

    dbScanUnlock((dbCommon*)crec);

    testDiag("Reset");
    testdbPutFieldOk("comp.RES", DBF_LONG, 0);

    dbScanLock((dbCommon*)crec);
    testOk1(crec->off==0);
    testOk1(crec->inx==0);
    testOk1(crec->nuse==0);
    checkArrD("comp", 0, 0, 0, 0, 0);
    dbScanUnlock((dbCommon*)crec);

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(compressTest)
{
    testPlan(116);
    testFIFOCirc();
    testLIFOCirc();
    return testDone();
}
