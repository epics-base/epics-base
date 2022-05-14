/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 * Lockset stress test.
 *
 * The test strategy is for N threads to contend for M records.
 * Each thread will perform one of three operations:
 * 1) Lock a single record.
 * 2) Lock several records.
 * 3) Retarget the TSEL link of a record
 *
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "envDefs.h"
#include "epicsEvent.h"
#include "epicsStdlib.h"
#include "epicsSpin.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsMath.h"
#include "dbCommon.h"
#include "epicsTime.h"

#include "dbLockPvt.h"
#include "dbStaticLib.h"

#include "dbUnitTest.h"
#include "testMain.h"

#include "dbAccess.h"
#include "errlog.h"

#include "xRecord.h"

#define testIntOk1(A, OP, B) testOk((A) OP (B), "%s (%d) %s %s (%d)", #A, A, #OP, #B, B);
#define testPtrOk1(A, OP, B) testOk((A) OP (B), "%s (%p) %s %s (%p)", #A, A, #OP, #B, B);

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

/* number of seconds for the test to run */
static double runningtime = 18.0;

/* number of worker threads */
static unsigned int nworkers = 5;

static unsigned int nrecords;

#define MAXLOCK 20

static dbCommon **precords;

typedef struct {
    int id;
    unsigned long N[3];
    double X[3];
    double X2[3];
    double min[3], max[3];

    unsigned int done;
    epicsEventId donevent;

    dbCommon *prec[MAXLOCK];
} workerPriv;

/* hopefully a uniform random number in [0.0, 1.0] */
static
double getRand(void)
{
    return rand()/(double)RAND_MAX;
}

static
void doSingle(workerPriv *p)
{
    size_t recn = (size_t)(getRand()*(nrecords-1));
    dbCommon *prec = precords[recn];
    xRecord *px = (xRecord*)prec;

    dbScanLock(prec);
    px->val++;
    dbScanUnlock(prec);
}

static
void doMulti(workerPriv *p)
{
    int sum = 0;
    size_t i;
    size_t nlock = 2 + (size_t)(getRand()*(MAXLOCK-3));
    size_t nrec = (size_t)(getRand()*(nrecords-1));
    dbLocker *locker;

    assert(nlock>=2);
    assert(nlock<nrecords);

    for(i=0; i<nlock; i++, nrec=(nrec+1)%nrecords) {
        p->prec[i] = precords[nrec];
    }

    locker = dbLockerAlloc(p->prec, nlock, 0);
    if(!locker)
        testAbort("locker allocation fails");

    dbScanLockMany(locker);
    for(i=0; i<nlock; i++) {
        xRecord *px = (xRecord*)p->prec[i];
        sum += px->val;
    }
    dbScanUnlockMany(locker);

    dbLockerFree(locker);
}

static
void doreTarget(workerPriv *p)
{
    char scratchsrc[60];
    char scratchdst[MAX_STRING_SIZE];
    long ret;
    DBADDR dbaddr;
    double action = getRand();
    size_t nsrc = (size_t)(getRand()*(nrecords-1));
    size_t ntarg = (size_t)(getRand()*(nrecords-1));
    xRecord *psrc = (xRecord*)precords[nsrc];
    xRecord *ptarg = (xRecord*)precords[ntarg];

    strcpy(scratchsrc, psrc->name);
    strcat(scratchsrc, ".TSEL");

    ret = dbNameToAddr(scratchsrc, &dbaddr);
    if(ret)
        testAbort("bad record name? %ld", ret);

    if(action<=0.6) {
        scratchdst[0] = '\0';
    } else {
        strcpy(scratchdst, ptarg->name);
    }

    ret = dbPutField(&dbaddr, DBR_STRING, ptarg->name, 1);
    if(ret)
        testAbort("put fails with %ld", ret);
}

static
void worker(void *raw)
{
    unsigned init = 1;
    workerPriv *priv = raw;

    testDiag("worker %d is %p", priv->id, epicsThreadGetIdSelf());

    while(!priv->done) {
        epicsUInt64 after, before;
        double sel = getRand();
        double duration;
        int act;

        before = epicsMonotonicGet();

        if(sel<0.33) {
            doSingle(priv);
            act = 0;
        } else if(sel<0.66) {
            doMulti(priv);
            act = 1;
        } else {
            doreTarget(priv);
            act = 2;
        }

        after = epicsMonotonicGet();
        duration = (after-before)*1e-9;

        priv->N[act]++;
        priv->X[act] += duration;
        priv->X2[act] += duration*duration;
        if(duration<priv->min[act] || init) {
            priv->min[act] = duration;
            init = 0;
        }
        if(duration>priv->max[act])
            priv->max[act] = duration;
    }

    epicsEventMustTrigger(priv->donevent);
}

MAIN(dbStressTest)
{
    DBENTRY ent;
    long status;
    unsigned int i;
    workerPriv *priv;
    char *nwork=getenv("NWORK");
    epicsTimeStamp seed;

    epicsTimeGetCurrent(&seed);

    srand(seed.nsec);

    if(nwork) {
        long val = 0;
        epicsParseLong(nwork, &val, 0, NULL);
        if(val>2)
            nworkers = val;
    }

    testPlan(80+nworkers*3);

#if defined(__rtems__)
    testSkip(80+nworkers*3, "Test assumes time sliced preempting scheduling");
    return testDone();
#endif

    priv = callocMustSucceed(nworkers, sizeof(*priv), "no memory");

    testDiag("lock set stress test");

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbStressLock.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* collect an array of all records */
    dbInitEntry(pdbbase, &ent);
    for(status = dbFirstRecordType(&ent);
        !status;
        status = dbNextRecordType(&ent))
    {
        for(status = dbFirstRecord(&ent);
            !status;
            status = dbNextRecord(&ent))
        {
            if(ent.precnode->flags&DBRN_FLAGS_ISALIAS)
                continue;
            nrecords++;
        }

    }
    if(nrecords<2)
        testAbort("where are the records!");
    precords = callocMustSucceed(nrecords, sizeof(*precords), "no mem");
    for(status = dbFirstRecordType(&ent), i = 0;
        !status;
        status = dbNextRecordType(&ent))
    {
        for(status = dbFirstRecord(&ent);
            !status;
            status = dbNextRecord(&ent))
        {
            if(ent.precnode->flags&DBRN_FLAGS_ISALIAS)
                continue;
            precords[i++] = ent.precnode->precord;
        }

    }
    dbFinishEntry(&ent);

    testDiag("Running with %u workers and %u records",
             nworkers, nrecords);

    for(i=0; i<nworkers; i++) {
        priv[i].id = i;
        priv[i].donevent = epicsEventMustCreate(epicsEventEmpty);
    }

    for(i=0; i<nworkers; i++) {
        epicsThreadMustCreate("runner", epicsThreadPriorityMedium,
                              epicsThreadGetStackSize(epicsThreadStackSmall),
                              &worker, &priv[i]);
    }

    testDiag("All started.  Will run for %f sec", runningtime);

    epicsThreadSleep(runningtime);

    testDiag("Stopping");

    for(i=0; i<nworkers; i++) {
        priv[i].done = 1;
    }

    for(i=0; i<nworkers; i++) {
        epicsEventMustWait(priv[i].donevent);
        epicsEventDestroy(priv[i].donevent);
    }

    testDiag("All stopped");

    testDiag("Validate lockSet ref counts");
    dbInitEntry(pdbbase, &ent);
    for(status = dbFirstRecordType(&ent);
        !status;
        status = dbNextRecordType(&ent))
    {
        for(status = dbFirstRecord(&ent);
            !status;
            status = dbNextRecord(&ent))
        {
            dbCommon *prec = ent.precnode->precord;
            lockSet *ls;
            if(ent.precnode->flags&DBRN_FLAGS_ISALIAS)
                continue;
            ls = prec->lset->plockSet;
            testOk(ellCount(&ls->lockRecordList)==ls->refcount, "%s only lockRecords hold refs. %d == %d",
                   prec->name,ellCount(&ls->lockRecordList),ls->refcount);
            testOk1(ls->ownerlocker==NULL);
        }

    }
    dbFinishEntry(&ent);

    testDiag("Statistics");
    for(i=0; i<nworkers; i++) {
        double avg[3], std[3];
        unsigned j;
        testDiag("Worker %u", i);
        for(j=0; j<3; j++) {
            avg[j] = priv[i].X[j]/priv[i].N[j];
            std[j] = sqrt( (priv[i].X2[j]/priv[i].N[j]) - avg[j]*avg[j] );
        }
        testDiag("N = %lu\t%lu\t%lu", priv[i].N[0], priv[i].N[1], priv[i].N[2]);
        testDiag("AVG = %g us\t%g us\t%g us", avg[0]*1e6, avg[1]*1e6, avg[2]*1e6);
        testDiag("STD = %g us\t%g us\t%g us", std[0]*1e6, std[1]*1e6, std[2]*1e6);
        testDiag("MIN = %g us\t%g us\t%g us", priv[i].min[0]*1e6, priv[i].min[1]*1e6, priv[i].min[2]*1e6);
        testDiag("MAX = %g us\t%g us\t%g us", priv[i].max[0]*1e6, priv[i].max[1]*1e6, priv[i].max[2]*1e6);

        testOk1(priv[i].N[0]>0);
        testOk1(priv[i].N[1]>0);
        testOk1(priv[i].N[2]>0);
    }

    testIocShutdownOk();

    testdbCleanup();

    free(priv);
    free(precords);

    return testDone();
}
