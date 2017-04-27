/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EPICS_DBCA_PRIVATE_API

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsEvent.h"
#include "iocInit.h"
#include "dbBase.h"
#include "link.h"
#include "dbAccess.h"
#include "epicsStdio.h"
#include "dbEvent.h"

/* Declarations from cadef.h and db_access.h which we can't include here */
typedef void * chid;
epicsShareExtern const unsigned short dbr_value_size[];
epicsShareExtern short epicsShareAPI ca_field_type (chid chan);
#define MAX_UNITS_SIZE		8

#include "dbCaPvt.h"
#include "errlog.h"
#include "testMain.h"

#include "xRecord.h"
#include "arrRecord.h"

#define testOp(FMT,A,OP,B) testOk((A)OP(B), #A " ("FMT") " #OP " " #B " ("FMT")", A,B)

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static epicsEventId waitEvent;
static unsigned waitCounter;

static
void waitForUpdateN(DBLINK *plink, unsigned long n)
{
    while(dbCaGetUpdateCount(plink)<n)
        epicsThreadSleep(0.01);
}

static
void putLink(DBLINK *plink, short dbr, const void*buf, long nReq)
{
    long ret;

    ret = dbPutLink(plink, dbr, buf, nReq);
    if(ret) {
        testFail("putLink fails %ld", ret);
    } else {
        testPass("putLink ok");
        dbCaSync();
    }
}

static long getTwice(struct link *psrclnk, void *dummy)
{
    epicsInt32 val1, val2;
    long status = dbGetLink(psrclnk, DBR_LONG, &val1, 0, 0);

    if (status) return status;

    epicsThreadSleep(0.5);
    status = dbGetLink(psrclnk, DBR_LONG, &val2, 0, 0);
    if (status) return status;

    testDiag("val1 = %d, val2 = %d", val1, val2);
    return (val1 == val2) ? 0 : -1;
}

static void countUp(void *parm)
{
    xRecord *ptarg = (xRecord *)parm;
    epicsInt32 val;

    for (val = 1; val < 10; val++) {
        dbScanLock((dbCommon*)ptarg);
        ptarg->val = val;
        db_post_events(ptarg, &ptarg->val, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
        dbScanUnlock((dbCommon*)ptarg);

        epicsThreadSleep(0.1);
    }

    if (waitEvent)
        epicsEventMustTrigger(waitEvent);
}

static void testNativeLink(void)
{
    xRecord *psrc, *ptarg;
    DBLINK *psrclnk;
    epicsInt32 temp;
    long nReq;

    testDiag("Link to a scalar numeric field");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest1.db", NULL, "TARGET=target CA");

    eltc(0);
    testIocInitOk();
    eltc(1);

    psrc = (xRecord*)testdbRecordPtr("source");
    ptarg= (xRecord*)testdbRecordPtr("target");
    psrclnk = &psrc->lnk;

    /* ensure this is really a CA link */
    testOk1(dbLockGetLockId((dbCommon*)psrc)!=dbLockGetLockId((dbCommon*)ptarg));

    testOk1(psrclnk->type==CA_LINK);

    waitForUpdateN(psrclnk, 1);

    dbScanLock((dbCommon*)ptarg);
    ptarg->val = 42;
    db_post_events(ptarg, &ptarg->val, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdateN(psrclnk, 2);

    dbScanLock((dbCommon*)psrc);
    /* local CA_LINK connects immediately */
    testOk1(dbCaIsLinkConnected(psrclnk));
    nReq = 422;
    testOk1(dbCaGetNelements(psrclnk, &nReq)==0);
    testOp("%ld",nReq,==,1l);

    nReq = 1;
    temp = 0x0f0f0f0f;
    testOk1(dbGetLink(psrclnk, DBR_LONG, (void*)&temp, NULL, &nReq)==0);
    testOp("%d",temp,==,42);
    dbScanUnlock((dbCommon*)psrc);

    temp = 1010;
    nReq = 1;
    putLink(psrclnk, DBR_LONG, (void*)&temp, nReq);

    dbScanLock((dbCommon*)ptarg);
    testOk1(ptarg->val==1010);
    dbScanUnlock((dbCommon*)ptarg);

    assert(!waitEvent);
    waitEvent = epicsEventMustCreate(epicsEventEmpty);

    /* Start counter */
    epicsThreadCreate("countUp", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall), countUp, ptarg);

    dbScanLock((dbCommon*)psrc);
    /* Check that unlocked gets change */
    temp = getTwice(psrclnk, NULL);
    testOk(temp == -1, "unlocked, getTwice returned %d (-1)", temp);

    /* Check locked gets are atomic */
    temp = dbLinkDoLocked(psrclnk, getTwice, NULL);
    testOk(temp == 0, "locked, getTwice returned %d (0)", temp);
    dbScanUnlock((dbCommon*)psrc);

    epicsEventMustWait(waitEvent);
    epicsEventDestroy(waitEvent);
    waitEvent = NULL;

    testIocShutdownOk();

    testdbCleanup();
}

static void testStringLink(void)
{
    xRecord *psrc, *ptarg;
    DBLINK *psrclnk;
    char temp[MAX_STRING_SIZE];
    long nReq;

    testDiag("Link to a string field");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest1.db", NULL, "TARGET=target.DESC CA");

    eltc(0);
    testIocInitOk();
    eltc(1);

    psrc = (xRecord*)testdbRecordPtr("source");
    ptarg= (xRecord*)testdbRecordPtr("target");
    psrclnk = &psrc->lnk;

    /* ensure this is really a CA link */
    testOk1(dbLockGetLockId((dbCommon*)psrc)!=dbLockGetLockId((dbCommon*)ptarg));

    testOk1(psrclnk->type==CA_LINK);

    waitForUpdateN(psrclnk, 1);

    dbScanLock((dbCommon*)ptarg);
    strcpy(ptarg->desc, "hello");
    db_post_events(ptarg, &ptarg->desc, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdateN(psrclnk, 2);

    dbScanLock((dbCommon*)psrc);
    /* local CA_LINK connects immediately */
    testOk1(dbCaIsLinkConnected(psrclnk));

    nReq = 1;
    memset(temp, '!', sizeof(temp));
    testOk1(dbGetLink(psrclnk, DBR_STRING, (void*)&temp, NULL, &nReq)==0);
    testOk(strcmp(temp, "hello")==0, "%s == hello", temp);
    dbScanUnlock((dbCommon*)psrc);

    strcpy(temp, "world");
    putLink(psrclnk, DBR_STRING, (void*)&temp, nReq);

    dbScanLock((dbCommon*)ptarg);
    testOk(strcmp(ptarg->desc, "world")==0, "%s == world", ptarg->desc);
    dbScanUnlock((dbCommon*)ptarg);

    testIocShutdownOk();

    testdbCleanup();
}

static void wasproc(xRecord *prec)
{
    waitCounter++;
    epicsEventTrigger(waitEvent);
}

static void testCP(void)
{
    xRecord *psrc, *ptarg;

    testDiag("Link CP modifier");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest1.db", NULL, "TARGET=target CP");

    psrc = (xRecord*)testdbRecordPtr("source");
    ptarg= (xRecord*)testdbRecordPtr("target");

    /* hook in before IOC init */
    waitCounter=0;
    psrc->clbk = &wasproc;

    assert(!waitEvent);
    waitEvent = epicsEventMustCreate(epicsEventEmpty);

    eltc(0);
    testIocInitOk();
    eltc(1);
    dbCaSync();

    epicsEventMustWait(waitEvent);

    dbScanLock((dbCommon*)psrc);
    testOp("%u",waitCounter,==,1); /* initial processing */
    dbScanUnlock((dbCommon*)psrc);

    dbScanLock((dbCommon*)ptarg);
    ptarg->val = 42;
    db_post_events(ptarg, &ptarg->val, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    epicsEventMustWait(waitEvent);

    dbScanLock((dbCommon*)psrc);
    testOp("%u",waitCounter,==,2); /* process due to monitor update */
    dbScanUnlock((dbCommon*)psrc);

    testIocShutdownOk();

    testdbCleanup();

    epicsEventDestroy(waitEvent);
    waitEvent = NULL;
}

static void fillArray(epicsInt32 *buf, unsigned count, epicsInt32 first)
{
    for(;count;count--,first++)
        *buf++ = first;
}

static void fillArrayDouble(double *buf, unsigned count, double first)
{
    for(;count;count--,first++)
        *buf++ = first;
}

static void checkArray(const char *msg,
                       epicsInt32 *buf, epicsInt32 first,
                       unsigned used)
{
    int match = 1;
    unsigned i;
    epicsInt32 x, *b;

    for(b=buf,x=first,i=0;i<used;i++,x++,b++)
        match &= (*b)==x;
    testOk(match, "%s", msg);
    if(!match) {
        for(b=buf,x=first,i=0;i<used;i++,x++,b++)
            if((*b)!=x)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
    }
}

static void checkArrayDouble(const char *msg,
                       double *buf, double first,
                       unsigned used)
{
    int match = 1;
    unsigned i;
    double x, *b;

    for(b=buf,x=first,i=0;i<used;i++,x++,b++)
        match &= (*b)==x;
    testOk(match, "%s", msg);
    if(!match) {
        for(b=buf,x=first,i=0;i<used;i++,x++,b++)
            if((*b)!=x)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
    }
}

static void spoilputbuf(DBLINK *lnk)
{
    caLink *pca = lnk->value.pv_link.pvt;

    if(lnk->type!=CA_LINK || !pca->pputNative)
        return;
    epicsMutexMustLock(pca->lock);
    memset(pca->pputNative, '!',
           pca->nelements*dbr_value_size[ca_field_type(pca->chid)]);
    epicsMutexUnlock(pca->lock);
}

static void testArrayLink(unsigned nsrc, unsigned ntarg)
{
    char buf[100];
    arrRecord *psrc, *ptarg;
    DBLINK *psrclnk;
    epicsInt32 *bufsrc, *buftarg, *tmpbuf;
    long nReq;
    unsigned num_min, num_max;

    testDiag("Link to a array numeric field");

    /* source.INP = "target CA" */
    epicsSnprintf(buf, sizeof(buf), "TARGET=target CA,FTVL=LONG,SNELM=%u,TNELM=%u",
                  nsrc, ntarg);
    testDiag("%s", buf);

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest2.db", NULL, buf);

    psrc = (arrRecord*)testdbRecordPtr("source");
    ptarg= (arrRecord*)testdbRecordPtr("target");
    psrclnk = &psrc->inp;

    eltc(0);
    testIocInitOk();
    eltc(1);

    waitForUpdateN(psrclnk, 1);

    bufsrc = psrc->bptr;
    buftarg= ptarg->bptr;

    num_max=num_min=psrc->nelm;
    if(num_min>ptarg->nelm)
        num_min=ptarg->nelm;
    if(num_max<ptarg->nelm)
        num_max=ptarg->nelm;
    /* always request more than can possibly be filled */
    num_max += 2;

    tmpbuf = callocMustSucceed(num_max, sizeof(*tmpbuf), "tmpbuf");

    dbScanLock((dbCommon*)ptarg);
    fillArray(buftarg, ptarg->nelm, 1);
    ptarg->nord = ptarg->nelm;
    db_post_events(ptarg, ptarg->bptr, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdateN(psrclnk, 2);

    dbScanLock((dbCommon*)psrc);
    testDiag("fetch source.INP into source.BPTR");
    nReq = psrc->nelm;
    if(dbGetLink(psrclnk, DBR_LONG, bufsrc, NULL, &nReq)==0) {
        testPass("dbGetLink");
        testOp("%ld",nReq,==,(long)num_min);
        checkArray("array update", bufsrc, 1, nReq);
    } else {
        testFail("dbGetLink");
        testSkip(2, "dbGetLink fails");
    }
    testDiag("fetch source.INP into temp buffer w/ larger capacity");
    nReq = num_max;
    if(dbGetLink(psrclnk, DBR_LONG, tmpbuf, NULL, &nReq)==0) {
        testPass("dbGetLink");
        testOp("%ld",nReq,==,(long)ntarg);
        checkArray("array update", tmpbuf, 1, nReq);
    } else {
        testFail("dbGetLink");
        testSkip(2, "dbGetLink fails");
    }
    dbScanUnlock((dbCommon*)psrc);

    fillArray(bufsrc, psrc->nelm, 2);
    /* write buffer allocated on first put */
    putLink(psrclnk, DBR_LONG, bufsrc, psrc->nelm);

    dbScanLock((dbCommon*)ptarg);
    testOp("%ld",(long)ptarg->nord,==,(long)num_min);

    dbScanUnlock((dbCommon*)ptarg);

    /* write again to ensure that buffer is completely updated */
    spoilputbuf(psrclnk);
    fillArray(bufsrc, psrc->nelm, 3);
    putLink(psrclnk, DBR_LONG, bufsrc, psrc->nelm);

    dbScanLock((dbCommon*)ptarg);
    testOp("%ld",(long)ptarg->nord,==,(long)num_min);
    checkArray("array update", buftarg, 3, num_min);
    dbScanUnlock((dbCommon*)ptarg);

    testIocShutdownOk();

    testdbCleanup();

    free(tmpbuf);
    /* records don't cleanup after themselves
     * so do here to silence valgrind
     */
    free(bufsrc);
    free(buftarg);
}


static void softarr(arrRecord *prec)
{
    long nReq = prec->nelm;
    long status = dbGetLink(&prec->inp, DBR_DOUBLE, prec->bptr, NULL, &nReq);

    if(status) {
        testFail("dbGetLink() -> %ld", status);
    } else {
        testPass("dbGetLink() succeeds");
        prec->nord = nReq;
        if(nReq>0)
            testDiag("%s.VAL[0] - %f", prec->name, *(double*)prec->bptr);
    }
    waitCounter++;
    epicsEventTrigger(waitEvent);
}

static void testreTargetTypeChange(void)
{
    arrRecord *psrc, *ptarg1, *ptarg2;
    double *bufsrc, *buftarg1;
    epicsInt32 *buftarg2;

    testDiag("Retarget an link to a PV with a different type DOUBLE->LONG");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest3.db", NULL, "NELM=5,TARGET=target1 CP");

    psrc = (arrRecord*)testdbRecordPtr("source");
    ptarg1= (arrRecord*)testdbRecordPtr("target1");
    ptarg2= (arrRecord*)testdbRecordPtr("target2");

    /* hook in before IOC init */
    waitCounter=0;
    psrc->clbk = &softarr;

    assert(!waitEvent);
    waitEvent = epicsEventMustCreate(epicsEventEmpty);

    eltc(0);
    testIocInitOk();
    eltc(1);

    epicsEventMustWait(waitEvent); /* wait for initial processing */

    bufsrc = psrc->bptr;
    buftarg1= ptarg1->bptr;
    buftarg2= ptarg2->bptr;

    testDiag("Update one with original target");

    dbScanLock((dbCommon*)ptarg2);
    fillArray(buftarg2, ptarg2->nelm, 2);
    ptarg2->nord = ptarg2->nelm;
    dbScanUnlock((dbCommon*)ptarg2);

    /* initialize buffers */
    dbScanLock((dbCommon*)ptarg1);
    fillArrayDouble(buftarg1, ptarg1->nelm, 1);
    ptarg1->nord = ptarg1->nelm;
    db_post_events(ptarg1, ptarg1->bptr, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg1);

    epicsEventMustWait(waitEvent); /* wait for update */

    dbScanLock((dbCommon*)psrc);
    testOp("%ld",(long)psrc->nord,==,(long)5);
    checkArrayDouble("array update", bufsrc, 1, 5);
    dbScanUnlock((dbCommon*)psrc);

    testDiag("Retarget");
    testdbPutFieldOk("source.INP", DBR_STRING, "target2 CP");

    epicsEventMustWait(waitEvent); /* wait for update */

    dbScanLock((dbCommon*)psrc);
    testOp("%ld",(long)psrc->nord,==,(long)5);
    checkArrayDouble("array update", bufsrc, 2, 5);
    dbScanUnlock((dbCommon*)psrc);

    testIocShutdownOk();

    testdbCleanup();

    /* records don't cleanup after themselves
     * so do here to silence valgrind
     */
    free(bufsrc);
    free(buftarg1);
    free(buftarg2);
}

void dbCaLinkTest_testCAC(void);

static void testCAC(void)
{
    arrRecord *psrc, *ptarg1, *ptarg2;
    double *bufsrc, *buftarg1;
    epicsInt32 *buftarg2;

    testDiag("Check local CA through libca");
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbCaLinkTest3.db", NULL, "NELM=5,TARGET=target1 CP");

    psrc = (arrRecord*)testdbRecordPtr("source");
    ptarg1= (arrRecord*)testdbRecordPtr("target1");
    ptarg2= (arrRecord*)testdbRecordPtr("target2");

    eltc(0);
    testIocInitOk();
    eltc(1);

    bufsrc = psrc->bptr;
    buftarg1= ptarg1->bptr;
    buftarg2= ptarg2->bptr;

    dbCaLinkTest_testCAC();

    testIocShutdownOk();

    testdbCleanup();

    /* records don't cleanup after themselves
     * so do here to silence valgrind
     */
    free(bufsrc);
    free(buftarg1);
    free(buftarg2);
}

MAIN(dbCaLinkTest)
{
    testPlan(101);
    testNativeLink();
    testStringLink();
    testCP();
    testArrayLink(1,1);
    testArrayLink(10,1);
    testArrayLink(1,10);
    testArrayLink(10,10);
    testreTargetTypeChange();
    testCAC();
    return testDone();
}
