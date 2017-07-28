/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "epicsThreadPool.h"

/* included to allow tests to peek */
#include "../../pool/poolPriv.h"

#include "testMain.h"
#include "epicsUnitTest.h"

#include "cantProceed.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"

/* Do nothing */
static void nullop(void)
{
    epicsThreadPool *pool;
    testDiag("nullop()");
    {
        epicsThreadPoolConfig conf;
        epicsThreadPoolConfigDefaults(&conf);
        testOk1(conf.maxThreads>0);

        testOk1((pool=epicsThreadPoolCreate(&conf))!=NULL);
        if(!pool)
            return;
    }

    epicsThreadPoolDestroy(pool);
}

/* Just create and destroy worker threads */
static void oneop(void)
{
    epicsThreadPool *pool;
    testDiag("oneop()");
    {
        epicsThreadPoolConfig conf;
        epicsThreadPoolConfigDefaults(&conf);
        conf.initialThreads=2;
        testOk1(conf.maxThreads>0);

        testOk1((pool=epicsThreadPoolCreate(&conf))!=NULL);
        if(!pool)
            return;
    }

    epicsThreadPoolDestroy(pool);
}

/* Test that Bursts of jobs will create enough threads to
 * run all in parallel
 */
typedef struct {
    epicsMutexId guard;
    unsigned int count;
    epicsEventId allrunning;
    epicsEventId done;
    epicsJob **job;
} countPriv;

static void countjob(void *param, epicsJobMode mode)
{
    countPriv *cnt=param;
    testOk1(mode==epicsJobModeRun||mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        return;

    epicsMutexMustLock(cnt->guard);
    testDiag("Job %lu", (unsigned long)cnt->count);
    cnt->count--;
    if(cnt->count==0) {
        testDiag("All jobs running");
        epicsEventSignal(cnt->allrunning);
    }
    epicsMutexUnlock(cnt->guard);

    epicsEventMustWait(cnt->done);
    epicsEventSignal(cnt->done); /* pass along to next thread */
}

/* Starts "mcnt" jobs in a pool with initial and max
 * thread counts "icnt" and "mcnt".
 * The test ensures that all jobs run in parallel.
 * "cork" checks the function of pausing the run queue
 * with epicsThreadPoolQueueRun
 */
static void postjobs(size_t icnt, size_t mcnt, int cork)
{
    size_t i;
    epicsThreadPool *pool;
    countPriv *priv=callocMustSucceed(1, sizeof(*priv), "postjobs priv alloc");
    priv->guard=epicsMutexMustCreate();
    priv->done=epicsEventMustCreate(epicsEventEmpty);
    priv->allrunning=epicsEventMustCreate(epicsEventEmpty);
    priv->count=mcnt;
    priv->job=callocMustSucceed(mcnt, sizeof(*priv->job), "postjobs job array");

    testDiag("postjobs(%lu,%lu)", (unsigned long)icnt, (unsigned long)mcnt);

    {
        epicsThreadPoolConfig conf;
        epicsThreadPoolConfigDefaults(&conf);
        conf.initialThreads=icnt;
        conf.maxThreads=mcnt;

        testOk1((pool=epicsThreadPoolCreate(&conf))!=NULL);
        if(!pool)
            return;
    }

    if(cork)
        epicsThreadPoolControl(pool, epicsThreadPoolQueueRun, 0);

    for(i=0; i<mcnt; i++) {
        testDiag("i=%lu", (unsigned long)i);
        priv->job[i] = epicsJobCreate(pool, &countjob, priv);
        testOk1(priv->job[i]!=NULL);
        testOk1(epicsJobQueue(priv->job[i])==0);
    }

    if(cork) {
        /* no jobs should have run */
        epicsMutexMustLock(priv->guard);
        testOk1(priv->count==mcnt);
        epicsMutexUnlock(priv->guard);

        epicsThreadPoolControl(pool, epicsThreadPoolQueueRun, 1);
    }

    testDiag("Waiting for all jobs to start");
    epicsEventMustWait(priv->allrunning);
    testDiag("Stop all");
    epicsEventSignal(priv->done);

    for(i=0; i<mcnt; i++) {
        testDiag("i=%lu", (unsigned long)i);
        epicsJobDestroy(priv->job[i]);
    }

    epicsThreadPoolDestroy(pool);
    epicsMutexDestroy(priv->guard);
    epicsEventDestroy(priv->allrunning);
    epicsEventDestroy(priv->done);
    free(priv->job);
    free(priv);
}

static unsigned int flag0 = 0;

/* Test cancel from job (no-op)
 * and destroy from job (lazy free)
 */
static void cleanupjob0(void* arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    testOk1(mode==epicsJobModeRun||mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        return;

    assert(flag0==0);
    flag0=1;

    testOk1(epicsJobQueue(job)==0);

    epicsJobDestroy(job); /* delete while job is running */
}
static void cleanupjob1(void* arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    testOk1(mode==epicsJobModeRun||mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        return;

    testOk1(epicsJobQueue(job)==0);

    testOk1(epicsJobUnqueue(job)==0);
    /* delete later after job finishes, but before pool is destroyed */
}
static void cleanupjob2(void* arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    testOk1(mode==epicsJobModeRun||mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        epicsJobDestroy(job); /* delete when threadpool is destroyed */
    else if(mode==epicsJobModeRun)
        testOk1(epicsJobUnqueue(job)==S_pool_jobIdle);
}
static epicsJobFunction cleanupjobs[3] = {&cleanupjob0,&cleanupjob1,&cleanupjob2};

/* Tests three methods for job cleanup.
 * 1. destroy which running
 * 2. deferred cleanup after pool destroyed
 * 3. immediate cleanup when pool destroyed
 */
static void testcleanup(void)
{
    int i=0;
    epicsThreadPool *pool;
    epicsJob *job[3];

    testDiag("testcleanup()");

    testOk1((pool=epicsThreadPoolCreate(NULL))!=NULL);
    if(!pool)
        return;

    /* unrolled so that valgrind can show which methods leaks */
    testOk1((job[0]=epicsJobCreate(pool, cleanupjobs[0], EPICSJOB_SELF))!=NULL);
    testOk1((job[1]=epicsJobCreate(pool, cleanupjobs[1], EPICSJOB_SELF))!=NULL);
    testOk1((job[2]=epicsJobCreate(pool, cleanupjobs[2], EPICSJOB_SELF))!=NULL);
    for(i=0; i<3; i++) {
        testOk1(epicsJobQueue(job[i])==0);
    }

    epicsThreadPoolWait(pool, -1);
    epicsJobDestroy(job[1]);
    epicsThreadPoolDestroy(pool);
}

/* Test re-add from inside job */
typedef struct {
    unsigned int count;
    epicsEventId done;
    epicsJob *job;
    unsigned int inprogress;
} readdPriv;

static void readdjob(void *arg, epicsJobMode mode)
{
    readdPriv *priv=arg;
    testOk1(mode==epicsJobModeRun||mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        return;
    testOk1(priv->inprogress==0);
    testDiag("count==%u", priv->count);

    if(priv->count--) {
        priv->inprogress=1;
        epicsJobQueue(priv->job);
        epicsThreadSleep(0.05);
        priv->inprogress=0;
    }else{
        epicsEventSignal(priv->done);
        epicsJobDestroy(priv->job);
    }
}

/* Test re-queueing a job while it is running.
 * Check that a single job won't run concurrently.
 */
static void testreadd(void) {
    epicsThreadPoolConfig conf;
    epicsThreadPool *pool;
    readdPriv *priv=callocMustSucceed(1, sizeof(*priv), "testcleanup priv");
    readdPriv *priv2=callocMustSucceed(1, sizeof(*priv), "testcleanup priv");

    testDiag("testreadd");

    priv->done=epicsEventMustCreate(epicsEventEmpty);
    priv->count=5;
    priv2->done=epicsEventMustCreate(epicsEventEmpty);
    priv2->count=5;

    epicsThreadPoolConfigDefaults(&conf);
    conf.maxThreads = 2;
    testOk1((pool=epicsThreadPoolCreate(&conf))!=NULL);
    if(!pool)
        return;

    testOk1((priv->job=epicsJobCreate(pool, &readdjob, priv))!=NULL);
    testOk1((priv2->job=epicsJobCreate(pool, &readdjob, priv2))!=NULL);

    testOk1(epicsJobQueue(priv->job)==0);
    testOk1(epicsJobQueue(priv2->job)==0);
    epicsEventMustWait(priv->done);
    epicsEventMustWait(priv2->done);

    testOk1(epicsThreadPoolNThreads(pool)==2);
    testDiag("epicsThreadPoolNThreads = %d", epicsThreadPoolNThreads(pool));

    epicsThreadPoolDestroy(pool);
    epicsEventDestroy(priv->done);
    epicsEventDestroy(priv2->done);
    free(priv);
    free(priv2);

}

static int shouldneverrun = 0;
static int numtoolate = 0;

/* test job canceling */
static
void neverrun(void *arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    testOk1(mode==epicsJobModeCleanup);
    if(mode==epicsJobModeCleanup)
        epicsJobDestroy(job);
    else
        shouldneverrun++;
}
static epicsEventId cancel[2];
static
void toolate(void *arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    if(mode==epicsJobModeCleanup){
        epicsJobDestroy(job);
        return;
    }
    testPass("Job runs");
    numtoolate++;
    epicsEventSignal(cancel[0]);
    epicsEventMustWait(cancel[1]);
}

static
void testcancel(void)
{
    epicsJob *job[2];
    epicsThreadPool *pool;
    testOk1((pool=epicsThreadPoolCreate(NULL))!=NULL);
    if(!pool)
        return;

    cancel[0]=epicsEventCreate(epicsEventEmpty);
    cancel[1]=epicsEventCreate(epicsEventEmpty);

    testOk1((job[0]=epicsJobCreate(pool, &neverrun, EPICSJOB_SELF))!=NULL);
    testOk1((job[1]=epicsJobCreate(pool, &toolate, EPICSJOB_SELF))!=NULL);

    /* freeze */
    epicsThreadPoolControl(pool, epicsThreadPoolQueueRun, 0);

    testOk1(epicsJobUnqueue(job[0])==S_pool_jobIdle); /* not queued yet */

    epicsJobQueue(job[0]);
    testOk1(epicsJobUnqueue(job[0])==0);
    testOk1(epicsJobUnqueue(job[0])==S_pool_jobIdle);

    epicsThreadSleep(0.01);
    epicsJobQueue(job[0]);
    testOk1(epicsJobUnqueue(job[0])==0);
    testOk1(epicsJobUnqueue(job[0])==S_pool_jobIdle);

    epicsThreadPoolControl(pool, epicsThreadPoolQueueRun, 1);

    epicsJobQueue(job[1]); /* actually let it run this time */

    epicsEventMustWait(cancel[0]);
    testOk1(epicsJobUnqueue(job[0])==S_pool_jobIdle);
    epicsEventSignal(cancel[1]);

    epicsThreadPoolDestroy(pool);
    epicsEventDestroy(cancel[0]);
    epicsEventDestroy(cancel[1]);

    testOk1(shouldneverrun==0);
    testOk1(numtoolate==1);
}

static
unsigned int sharedWasDeleted=0;

static
void lastjob(void *arg, epicsJobMode mode)
{
    epicsJob *job=arg;
    if(mode==epicsJobModeCleanup) {
        sharedWasDeleted=1;
        epicsJobDestroy(job);
    }
}

static
void testshared(void)
{
    epicsThreadPool *poolA, *poolB;
    epicsThreadPoolConfig conf;
    epicsJob *job;

    epicsThreadPoolConfigDefaults(&conf);

    testDiag("Check reference counting of shared pools");

    testOk1((poolA=epicsThreadPoolGetShared(&conf))!=NULL);

    testOk1(poolA->sharedCount==1);

    testOk1((poolB=epicsThreadPoolGetShared(&conf))!=NULL);

    testOk1(poolA==poolB);

    testOk1(poolA->sharedCount==2);

    epicsThreadPoolReleaseShared(poolA);

    testOk1(poolB->sharedCount==1);

    testOk1((job=epicsJobCreate(poolB, &lastjob, EPICSJOB_SELF))!=NULL);

    epicsThreadPoolReleaseShared(poolB);

    testOk1(sharedWasDeleted==1);

    testOk1((poolA=epicsThreadPoolGetShared(&conf))!=NULL);

    testOk1(poolA->sharedCount==1);

    epicsThreadPoolReleaseShared(poolA);

}

MAIN(epicsThreadPoolTest)
{
    testPlan(171);

    nullop();
    oneop();
    testDiag("Queue with delayed start");
    postjobs(1,1,1);
    postjobs(0,1,1);
    postjobs(4,4,1);
    postjobs(0,4,1);
    postjobs(2,4,1);
    testDiag("Queue with immediate start");
    postjobs(1,1,0);
    postjobs(0,1,0);
    postjobs(4,4,0);
    postjobs(0,4,0);
    postjobs(2,4,0);
    testcleanup();
    testreadd();
    testcancel();
    testshared();

    return testDone();
}
