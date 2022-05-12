/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>

#include "cantProceed.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "dbLock.h"
#include "errlog.h"
#include "dbAccess.h"
#include "epicsMath.h"
#include "menuYesNo.h"

#include "aiRecord.h"
#include "waveformRecord.h"
#include "compressRecord.h"

#define testDEq(A,B,D) testOk(fabs((A)-(B))<(D), #A " (%f) ~= " #B " (%f)", A, B)
#define fetchRecordOrDie(recname, addr)     if (dbNameToAddr(recname, &addr)) {testAbort("Unknown PV '%s'", recname);}


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

    fetchRecordOrDie(pv, addr);

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

    fetchRecordOrDie(pv, addr);

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

void
writeToWaveform(DBADDR *addr, long count, ...) {
    va_list args;
    long i;
    double *values = (double *)callocMustSucceed(count, sizeof(double), "writeToWaveform");

    va_start(args, count);
    for (i=0; i< count; i++) {
        values[i] = va_arg(args, double);
    }
    va_end(args);

    dbScanLock(addr->precord);
    dbPut(addr, DBR_DOUBLE, values, count);
    dbScanUnlock(addr->precord);
    free(values);
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

    testdbReadDatabase("compressTest.db", NULL, "INP=ai,ALG=Circular Buffer,BALG=FIFO Buffer,NSAM=4");

    vrec = (aiRecord*)testdbRecordPtr("ai");
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
        "INP=ai,ALG=Circular Buffer,BALG=LIFO Buffer,NSAM=4");

    vrec = (aiRecord*)testdbRecordPtr("ai");
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

void
testArrayAverage(void) {
    DBADDR wfaddr, caddr;

    testDiag("Test Array Average");
    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("compressTest.db", NULL, "INP=wf,ALG=Average,BALG=FIFO Buffer,NSAM=4,N=2");

    eltc(0);
    testIocInitOk();
    eltc(1);

    fetchRecordOrDie("wf", wfaddr);
    fetchRecordOrDie("comp", caddr);

    writeToWaveform(&wfaddr, 4, 1., 2., 3., 4.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);

    writeToWaveform(&wfaddr, 4, 2., 4., 6., 8.);

    dbProcess(caddr.precord);

    checkArrD("comp", 4, 1.5, 3., 4.5, 6.);
    dbScanUnlock(caddr.precord);

    testIocShutdownOk();
    testdbCleanup();
}

void
testNto1Average(void) {
    double buf = 0.0;
    long nReq = 1;
    DBADDR wfaddr, caddr;

    testDiag("Test Average");

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("compressTest.db", NULL, "INP=wf,ALG=N to 1 Average,BALG=FIFO Buffer,NSAM=1,N=4");

    eltc(0);
    testIocInitOk();
    eltc(1);

    fetchRecordOrDie("wf", wfaddr);
    fetchRecordOrDie("comp", caddr);

    testDiag("Test incomplete input data");

    writeToWaveform(&wfaddr, 3, 1., 2., 3.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testOk1(nReq == 0);
    testDEq(buf, 0., 0.01);
    dbScanUnlock(caddr.precord);

    testDiag("Test complete input data");

    writeToWaveform(&wfaddr, 4, 1., 2., 3., 4.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    nReq = 1;
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 2.5, 0.01);
    dbScanUnlock(caddr.precord);

    testDiag("Test single input data");

    writeToWaveform(&wfaddr, 1, 5.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    nReq = 1;
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    // Assert that nothing has changed from before
    testDEq(buf, 2.5, 0.01);
    dbScanUnlock(caddr.precord);

    testIocShutdownOk();
    testdbCleanup();
}

void
testNto1AveragePartial(void) {
    double buf = 0.0;
    long nReq = 1;
    DBADDR wfaddr, caddr;

    testDiag("Test Average, Partial");

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("compressTest.db", NULL, "INP=wf,ALG=N to 1 Average,BALG=FIFO Buffer,NSAM=1,N=4,PBUF=YES");

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Test incomplete input data");

    fetchRecordOrDie("wf", wfaddr);
    fetchRecordOrDie("comp", caddr);

    writeToWaveform(&wfaddr, 3, 1., 2., 3.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 2.0, 0.01);
    dbScanUnlock(caddr.precord);

    testDiag("Test single entry from wf record");

    writeToWaveform(&wfaddr, 1, 6.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 6.0, 0.01);
    dbScanUnlock(caddr.precord);

    testIocShutdownOk();
    testdbCleanup();
}

void
testNto1LowValue(void) {
    double buf = 0.0;
    long nReq = 1;
    DBADDR wfaddr, caddr;

    testDiag("Test 'N to 1 Low Value'");

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("compressTest.db", NULL, "INP=wf,ALG=N to 1 Low Value,BALG=FIFO Buffer,NSAM=1,N=4");

    eltc(0);
    testIocInitOk();
    eltc(1);

    fetchRecordOrDie("wf", wfaddr);
    fetchRecordOrDie("comp", caddr);

    testDiag("Test full array");

    writeToWaveform(&wfaddr, 4, 1., 2., 3., 4.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 1.0, 0.01);
    dbScanUnlock(caddr.precord);

    writeToWaveform(&wfaddr, 4, 4., 3., 2., 1.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 1.0, 0.01);
    dbScanUnlock(caddr.precord);

    testDiag("Test partial data with PBUF set to NO");

    writeToWaveform(&wfaddr, 3, 5., 6., 7.);

    dbScanLock(caddr.precord);
    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    // We confirm that this hasn't changed i.e. the dbProcess above did nothing
    testDEq(buf, 1.0, 0.01);

    testDiag("Test partial data with PBUF set to YES");

    ((compressRecord *)caddr.precord)->pbuf = menuYesNoYES;

    dbProcess(caddr.precord);
    if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
        testAbort("dbGet failed on compress record");

    testDEq(buf, 5.0, 0.01);
    dbScanUnlock(caddr.precord);


    testIocShutdownOk();
    testdbCleanup();
}

void
testAIAveragePartial(void) {
    double buf = 0.;
    double data[5] = {1., 2., 3., 4., 5.};
    /* 
     * Note that the fifth dbPut essentially resets the circular buffer, so the
     * average is once again the average of the _first_ entry alone.
     */
    double expected[5] = {1., 1.5, 2., 2.5, 5.};
    long nReq = 1;
    int i;
    DBADDR aiaddr, caddr;

    testDiag("Test 'N to 1 Average' with analog in, PBUF=YES");

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("compressTest.db", NULL, "INP=ai,ALG=N to 1 Average,BALG=FIFO Buffer,NSAM=1,N=4,PBUF=YES");

    eltc(0);
    testIocInitOk();
    eltc(1);

    fetchRecordOrDie("ai", aiaddr);
    fetchRecordOrDie("comp", caddr);

    for (i = 0; i < 5; i++) {
        dbScanLock(aiaddr.precord);
        dbPut(&aiaddr, DBR_DOUBLE, &data[i], 1);
        dbScanUnlock(aiaddr.precord);

        dbScanLock(caddr.precord);
        dbProcess(caddr.precord);
        if (dbGet(&caddr, DBR_DOUBLE, &buf, NULL, &nReq, NULL))
            testAbort("dbGet failed on compress record");
        dbScanUnlock(caddr.precord);

        testDEq(buf, expected[i], 0.01);
    }

    testIocShutdownOk();
    testdbCleanup();
}

MAIN(compressTest)
{
    testPlan(132);
    testFIFOCirc();
    testLIFOCirc();
    testArrayAverage();
    testNto1Average();
    testNto1AveragePartial();
    testAIAveragePartial();
    testNto1LowValue();
    return testDone();
}
