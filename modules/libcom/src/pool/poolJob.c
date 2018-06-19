/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define epicsExportSharedSymbols

#include "dbDefs.h"
#include "errlog.h"
#include "ellLib.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsInterrupt.h"

#include "epicsThreadPool.h"
#include "poolPriv.h"

void *epicsJobArgSelfMagic = &epicsJobArgSelfMagic;

static
void workerMain(void *arg)
{
    epicsThreadPool *pool = arg;
    unsigned int nrun, ocnt;

    /* workers are created with counts
     * in the running, sleeping, and (possibly) waking counters
     */

    epicsMutexMustLock(pool->guard);
    pool->threadsAreAwake++;
    pool->threadsSleeping--;

    while (1) {
        ELLNODE *cur;

        pool->threadsAreAwake--;
        pool->threadsSleeping++;
        epicsMutexUnlock(pool->guard);

        epicsEventMustWait(pool->workerWakeup);

        epicsMutexMustLock(pool->guard);
        pool->threadsSleeping--;
        pool->threadsAreAwake++;

        if (pool->threadsWaking==0)
            continue;

        pool->threadsWaking--;

        CHECKCOUNT(pool);

        if (pool->shutdown)
            break;

        if (pool->pauserun)
            continue;

        /* more threads to wakeup */
        if (pool->threadsWaking) {
            epicsEventSignal(pool->workerWakeup);
        }

        while ((cur=ellGet(&pool->jobs)) != NULL) {
            epicsJob *job = CONTAINER(cur, epicsJob, jobnode);

            assert(job->queued && !job->running);

            job->queued=0;
            job->running=1;

            epicsMutexUnlock(pool->guard);
            (*job->func)(job->arg, epicsJobModeRun);
            epicsMutexMustLock(pool->guard);

            if (job->freewhendone) {
                job->dead=1;
                free(job);
            }
            else {
                job->running=0;
                /* job may be re-queued from within callback */
                if (job->queued)
                    ellAdd(&pool->jobs, &job->jobnode);
                else
                    ellAdd(&pool->owned, &job->jobnode);
            }
        }

        if (pool->observerCount)
            epicsEventSignal(pool->observerWakeup);
    }

    pool->threadsAreAwake--;
    pool->threadsRunning--;

    nrun = pool->threadsRunning;
    ocnt = pool->observerCount;
    epicsMutexUnlock(pool->guard);

    if (ocnt)
        epicsEventSignal(pool->observerWakeup);

    if (nrun)
        epicsEventSignal(pool->workerWakeup); /* pass along */
    else
        epicsEventSignal(pool->shutdownEvent);
}

int createPoolThread(epicsThreadPool *pool)
{
    epicsThreadId tid;

    tid = epicsThreadCreate("PoolWorker",
                            pool->conf.workerPriority,
                            pool->conf.workerStack,
                            &workerMain,
                            pool);
    if (!tid)
        return S_pool_noThreads;

    pool->threadsRunning++;
    pool->threadsSleeping++;
    return 0;
}

epicsJob* epicsJobCreate(epicsThreadPool *pool,
                         epicsJobFunction func,
                         void *arg)
{
    epicsJob *job = calloc(1, sizeof(*job));

    if (!job)
        return NULL;

    if (arg == &epicsJobArgSelfMagic)
        arg = job;

    job->pool = NULL;
    job->func = func;
    job->arg = arg;

    epicsJobMove(job, pool);

    return job;
}

void epicsJobDestroy(epicsJob *job)
{
    epicsThreadPool *pool;
    if (!job || !job->pool) {
        free(job);
        return;
    }
    pool = job->pool;

    epicsMutexMustLock(pool->guard);

    assert(!job->dead);

    epicsJobUnqueue(job);

    if (job->running || job->freewhendone) {
        job->freewhendone = 1;
    }
    else {
        ellDelete(&pool->owned, &job->jobnode);
        job->dead = 1;
        free(job);
    }

    epicsMutexUnlock(pool->guard);
}

int epicsJobMove(epicsJob *job, epicsThreadPool *newpool)
{
    epicsThreadPool *pool = job->pool;

    /* remove from current pool */
    if (pool) {
        epicsMutexMustLock(pool->guard);

        if (job->queued || job->running) {
            epicsMutexUnlock(pool->guard);
            return S_pool_jobBusy;
        }

        ellDelete(&pool->owned, &job->jobnode);

        epicsMutexUnlock(pool->guard);
    }

    pool = job->pool = newpool;

    /* add to new pool */
    if (pool) {
        epicsMutexMustLock(pool->guard);

        ellAdd(&pool->owned, &job->jobnode);

        epicsMutexUnlock(pool->guard);
    }

    return 0;
}

int epicsJobQueue(epicsJob *job)
{
    int ret = 0;
    epicsThreadPool *pool = job->pool;

    if (!pool)
        return S_pool_noPool;

    epicsMutexMustLock(pool->guard);

    assert(!job->dead);

    if (pool->pauseadd) {
        ret = S_pool_paused;
        goto done;
    }
    else if (job->freewhendone) {
        ret = S_pool_jobBusy;
        goto done;
    }
    else if (job->queued) {
        goto done;
    }

    job->queued = 1;
    /* Job may be queued from within a callback */
    if (!job->running) {
        ellDelete(&pool->owned, &job->jobnode);
        ellAdd(&pool->jobs, &job->jobnode);
    }
    else {
        /* some worker will find it again before sleeping */
        goto done;
    }

    /* Since we hold the lock, we can be certain that all awake worker are
     * executing work functions.  The current thread may be a worker.
     * We prefer to wakeup a new worker rather then wait for a busy worker to
     * finish.  However, after we initiate a wakeup there will be a race
     * between the worker waking up, and a busy worker finishing.
     * Thus we can't avoid spurious wakeups.
     */

    if (pool->threadsRunning >= pool->conf.maxThreads) {
        /* all workers created... */
        /* ... but some are sleeping, so wake one up */
        if (pool->threadsWaking < pool->threadsSleeping) {
            pool->threadsWaking++;
            epicsEventSignal(pool->workerWakeup);
        }
        /*else one of the running workers will find this job before sleeping */
        CHECKCOUNT(pool);

    }
    else {
        /* could create more workers so
         * will either create a new worker, or wakeup an existing worker
         */

        if (pool->threadsWaking >= pool->threadsSleeping) {
            /* all sleeping workers have already been woken.
             * start a new worker for this job
             */
            if (createPoolThread(pool) && pool->threadsRunning == 0) {
                /* oops, we couldn't lazy create our first worker
                 * so this job would never run!
                 */
                ret = S_pool_noThreads;
                job->queued = 0;
                /* if threadsRunning==0 then no jobs can be running */
                assert(!job->running);
                ellDelete(&pool->jobs, &job->jobnode);
                ellAdd(&pool->owned, &job->jobnode);
            }
        }
        if (ret == 0) {
            pool->threadsWaking++;
            epicsEventSignal(pool->workerWakeup);
        }
        CHECKCOUNT(pool);
    }

done:
    epicsMutexUnlock(pool->guard);
    return ret;
}

int epicsJobUnqueue(epicsJob *job)
{
    int ret = S_pool_jobIdle;
    epicsThreadPool *pool = job->pool;

    if (!pool)
        return S_pool_noPool;

    epicsMutexMustLock(pool->guard);

    assert(!job->dead);

    if (job->queued) {
        if (!job->running) {
            ellDelete(&pool->jobs, &job->jobnode);
            ellAdd(&pool->owned, &job->jobnode);
        }
        job->queued = 0;
        ret = 0;
    }

    epicsMutexUnlock(pool->guard);

    return ret;
}

