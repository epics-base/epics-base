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

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "iocInit.h"
#include "dbBase.h"
#include "link.h"
#include "dbAccess.h"
#include "epicsStdio.h"
#include "dbEvent.h"
/* hackish duplication since we can't include db_access.h here */
typedef void* chid;
#define MAX_UNITS_SIZE		8
#include "dbCaPvt.h"
#include "errlog.h"

#include "xRecord.h"
#include "arrRecord.h"

#include "testMain.h"

#define testOp(FMT,A,OP,B) testOk((A)OP(B), #A " ("FMT") " #OP " " #B " ("FMT")", A,B)

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static epicsEventId waitEvent;
static unsigned waitCounter;

static
void waitCB(void *unused)
{
    if(waitEvent)
        epicsEventMustTrigger(waitEvent);
    waitCounter++; //TODO: atomic
}

static
void startWait(DBLINK *plink)
{
    caLink *pca = plink->value.pv_link.pvt;

    assert(!waitEvent);
    waitEvent = epicsEventMustCreate(epicsEventEmpty);

    assert(pca);
    epicsMutexMustLock(pca->lock);
    assert(!pca->monitor && !pca->userPvt);
    pca->monitor = &waitCB;
    epicsMutexUnlock(pca->lock);
    testDiag("Preparing to wait on pca=%p", pca);
}

static
void waitForUpdate(DBLINK *plink)
{
    caLink *pca = plink->value.pv_link.pvt;

    assert(pca);

    testDiag("Waiting on pca=%p", pca);
    epicsEventMustWait(waitEvent);

    epicsMutexMustLock(pca->lock);
    pca->monitor = NULL;
    pca->userPvt = NULL;
    epicsMutexUnlock(pca->lock);

    epicsEventDestroy(waitEvent);
    waitEvent = NULL;
}

static
void putLink(DBLINK *plink, short dbr, const void*buf, long nReq)
{
    long ret;
    waitEvent = epicsEventMustCreate(epicsEventEmpty);

    ret = dbCaPutLinkCallback(plink, dbr, buf, nReq,
                              &waitCB, NULL);
    if(ret) {
        testFail("putLink fails %ld\n", ret);
    } else {
        epicsEventMustWait(waitEvent);
        testPass("putLink ok\n");
    }
    epicsEventDestroy(waitEvent);
    waitEvent = NULL;
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

    // ensure this is really a CA link
    testOk1(dbLockGetLockId((dbCommon*)psrc)!=dbLockGetLockId((dbCommon*)ptarg));

    testOk1(psrclnk->type==CA_LINK);

    startWait(psrclnk);

    dbScanLock((dbCommon*)ptarg);
    ptarg->val = 42;
    db_post_events(ptarg, &ptarg->val, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdate(psrclnk);

    dbScanLock((dbCommon*)psrc);
    // local CA_LINK connects immediately
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

    // ensure this is really a CA link
    testOk1(dbLockGetLockId((dbCommon*)psrc)!=dbLockGetLockId((dbCommon*)ptarg));

    testOk1(psrclnk->type==CA_LINK);

    startWait(psrclnk);

    dbScanLock((dbCommon*)ptarg);
    strcpy(ptarg->desc, "hello");
    db_post_events(ptarg, &ptarg->desc, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdate(psrclnk);

    dbScanLock((dbCommon*)psrc);
    // local CA_LINK connects immediately
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
    waitCB(NULL);
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

    epicsEventMustWait(waitEvent);

    dbScanLock((dbCommon*)psrc);
    testOp("%u",waitCounter,==,1); // initial processing
    dbScanUnlock((dbCommon*)psrc);

    dbScanLock((dbCommon*)ptarg);
    ptarg->val = 42;
    db_post_events(ptarg, &ptarg->val, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    epicsEventMustWait(waitEvent);

    dbScanLock((dbCommon*)psrc);
    testOp("%u",waitCounter,==,2); // process due to monitor update
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
                       unsigned used, unsigned total)
{
    int match = 1;
    unsigned i;
    epicsInt32 x, *b;
    for(b=buf,x=first,i=0;i<used;i++,x++,b++)
        match &= (*b)==x;
    for(;i<total;i++,x++,b++)
        match &= (*b)==0;
    testOk(match, msg);
    if(!match) {
        for(b=buf,x=first,i=0;i<used;i++,x++,b++)
            if((*b)!=x)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
        for(;i<total;i++,x++,b++)
            if((*b)!=0)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
    }
}

static void checkArrayDouble(const char *msg,
                       double *buf, double first,
                       unsigned used, unsigned total)
{
    int match = 1;
    unsigned i;
    double x, *b;
    for(b=buf,x=first,i=0;i<used;i++,x++,b++)
        match &= (*b)==x;
    for(;i<total;i++,x++,b++)
        match &= (*b)==0;
    testOk(match, msg);
    if(!match) {
        for(b=buf,x=first,i=0;i<used;i++,x++,b++)
            if((*b)!=x)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
        for(;i<total;i++,x++,b++)
            if((*b)!=0)
                testDiag("%u %u != %u", i, (unsigned)*b, (unsigned)x);
    }
}

static void spoilputbuf(DBLINK *lnk)
{
    extern const unsigned short dbr_value_size[];
    short epicsShareAPI ca_field_type (chid chan);

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
    epicsInt32 *bufsrc, *buftarg;
    long nReq;
    unsigned num;

    testDiag("Link to a array numeric field");

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

    bufsrc = psrc->bptr;
    buftarg= ptarg->bptr;

    num=psrc->nelm;
    if(num>ptarg->nelm)
        num=ptarg->nelm;

    startWait(psrclnk);

    dbScanLock((dbCommon*)ptarg);
    fillArray(buftarg, ptarg->nelm, 1);
    ptarg->nord = ptarg->nelm;
    db_post_events(ptarg, ptarg->bptr, DBE_VALUE|DBE_ALARM|DBE_ARCHIVE);
    dbScanUnlock((dbCommon*)ptarg);

    waitForUpdate(psrclnk);

    dbScanLock((dbCommon*)psrc);
    nReq = psrc->nelm;
    if(dbGetLink(psrclnk, DBR_LONG, bufsrc, NULL, &nReq)==0) {
        testPass("dbGetLink");
        testOp("%ld",nReq,==,(long)num);
        checkArray("array update", bufsrc, 1, nReq, psrc->nelm);
    } else {
        testFail("dbGetLink");
        testSkip(2, "dbGetLink fails");
    }
    dbScanUnlock((dbCommon*)psrc);

    fillArray(bufsrc, psrc->nelm, 2);
    /* write buffer allocated on first put */
    putLink(psrclnk, DBR_LONG, bufsrc, psrc->nelm);

    dbScanLock((dbCommon*)ptarg);
    /* CA links always write the full target array length */
    testOp("%ld",(long)ptarg->nord,==,(long)ptarg->nelm);
    /* However, if the source length is less, then the target
     * is zero filled
     */
    checkArray("array update", buftarg, 2, num, ptarg->nelm);
    dbScanUnlock((dbCommon*)ptarg);

    /* write again to ensure that buffer is completely updated */
    spoilputbuf(psrclnk);
    fillArray(bufsrc, psrc->nelm, 3);
    putLink(psrclnk, DBR_LONG, bufsrc, psrc->nelm);

    dbScanLock((dbCommon*)ptarg);
    testOp("%ld",(long)ptarg->nord,==,(long)ptarg->nelm);
    checkArray("array update", buftarg, 3, num, ptarg->nelm);
    dbScanUnlock((dbCommon*)ptarg);

    testIocShutdownOk();

    testdbCleanup();

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
    waitCB(NULL);
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

    epicsEventMustWait(waitEvent); // wait for initial processing

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

    epicsEventMustWait(waitEvent); // wait for update

    dbScanLock((dbCommon*)psrc);
    testOp("%ld",(long)psrc->nord,==,(long)5);
    checkArrayDouble("array update", bufsrc, 1, 5, psrc->nelm);
    dbScanUnlock((dbCommon*)psrc);

    testDiag("Retarget");
    testdbPutFieldOk("source.INP", DBR_STRING, "target2 CP");

    epicsEventMustWait(waitEvent); // wait for update

    dbScanLock((dbCommon*)psrc);
    testOp("%ld",(long)psrc->nord,==,(long)5);
    checkArrayDouble("array update", bufsrc, 2, 5, psrc->nelm);
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
    testPlan(85);
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
