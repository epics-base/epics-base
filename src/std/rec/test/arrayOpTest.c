/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "dbTest.h"

#include "dbUnitTest.h"
#include "errlog.h"

#include "waveformRecord.h"

#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void testGetPutArray(void)
{
    double data[4] = {11, 12, 13, 14};
    DBADDR addr, save;
    long nreq;
    epicsInt32 *pbtr;
    waveformRecord *prec;

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("arrayOpTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Test dbGet() and dbPut() from/to an array");

    prec = (waveformRecord*)testdbRecordPtr("wfrec");
    if(!prec || dbNameToAddr("wfrec", &addr))
        testAbort("Failed to find record wfrec");
    memcpy(&save, &addr, sizeof(save));

    testDiag("Fetch initial value of %s", prec->name);

    dbScanLock(addr.precord);
    testOk(prec->nord==3, "prec->nord==3 (got %d)", prec->nord);

    nreq = NELEMENTS(data);
    if(dbGet(&addr, DBF_DOUBLE, &data, NULL, &nreq, NULL)) {
        testFail("dbGet fails");
        testSkip(1, "failed get");
    } else {
        testOk(nreq==3, "nreq==3 (got %ld)", nreq);
        testOk1(data[0]==1.0 && data[1]==2.0 && data[2]==3.0);
    }
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);
    addr=save;

    testDiag("Write a new value");

    data[0] = 4.0;
    data[1] = 5.0;
    data[2] = 6.0;
    data[3] = 7.0;

    dbScanLock(addr.precord);
    testOk1(dbPut(&addr, DBF_DOUBLE, &data, NELEMENTS(data))==0);
    pbtr = prec->bptr;
    testOk(prec->nord==4, "prec->nord==4 (got %u)", prec->nord);
    testOk1(pbtr[0]==4 && pbtr[1]==5 && pbtr[2]==6 && pbtr[3]==7);
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);
    addr=save;

    memset(&data, 0, sizeof(data));

    testDiag("Reread the value");

    dbScanLock(addr.precord);
    nreq = NELEMENTS(data);
    if(dbGet(&addr, DBF_DOUBLE, &data, NULL, &nreq, NULL))
        testFail("dbGet fails");
    else {
        testOk1(nreq==NELEMENTS(data));
        testOk1(data[0]==4.0 && data[1]==5.0 && data[2]==6.0 && data[3]==7.0);
    }
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);

    testDiag("Test dbGet() and dbPut() from/to an array of size 1");

    prec = (waveformRecord*)testdbRecordPtr("wfrec1");
    if(!prec || dbNameToAddr("wfrec1", &addr))
        testAbort("Failed to find record wfrec1");
    memcpy(&save, &addr, sizeof(save));

    testDiag("Fetch initial value of %s", prec->name);

    dbScanLock(addr.precord);
    testOk(prec->nord==0, "prec->nord==0 (got %d)", prec->nord);

    nreq = NELEMENTS(data);
    if(dbGet(&addr, DBF_DOUBLE, &data, NULL, &nreq, NULL))
        testFail("dbGet fails");
    else {
        testOk(nreq==0, "nreq==0 (got %ld)", nreq);
    }
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);
    addr=save;

    testDiag("Write a new value");

    data[0] = 4.0;
    data[1] = 5.0;
    data[2] = 6.0;
    data[3] = 7.0;

    dbScanLock(addr.precord);
    testOk1(dbPut(&addr, DBF_DOUBLE, &data, 1)==0);
    pbtr = prec->bptr;
    testOk(prec->nord==1, "prec->nord==1 (got %u)", prec->nord);
    testOk1(pbtr[0]==4);
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);
    addr=save;

    memset(&data, 0, sizeof(data));

    testDiag("Reread the value");

    dbScanLock(addr.precord);
    nreq = NELEMENTS(data);
    if(dbGet(&addr, DBF_DOUBLE, &data, NULL, &nreq, NULL))
        testFail("dbGet fails");
    else {
        testOk1(nreq==1);
        testOk1(data[0]==4.0);
    }
    dbScanUnlock(addr.precord);

    testOk1(memcmp(&addr, &save, sizeof(save))==0);

    testIocShutdownOk();

    testdbCleanup();
}

MAIN(arrayOpTest)
{
    testPlan(21);
    testGetPutArray();
    return testDone();
}
