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
#include "cantProceed.h"

#include "epicsThreadPool.h"
#include "poolPriv.h"


void epicsThreadPoolConfigDefaults(epicsThreadPoolConfig *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->maxThreads = epicsThreadGetCPUs();
    opts->workerStack = epicsThreadGetStackSize(epicsThreadStackSmall);

    if (epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityCAServerHigh, &opts->workerPriority)
            != epicsThreadBooleanStatusSuccess)
        opts->workerPriority = epicsThreadPriorityMedium;
}

epicsThreadPool* epicsThreadPoolCreate(epicsThreadPoolConfig *opts)
{
    size_t i;
    epicsThreadPool *pool;

    /* caller likely didn't initialize the options structure */
    if (opts && opts->maxThreads == 0) {
        errlogMessage("Error: epicsThreadPoolCreate() options provided, but not initialized");
        return NULL;
    }

    pool = calloc(1, sizeof(*pool));
    if (!pool)
        return NULL;

    if (opts)
        memcpy(&pool->conf, opts, sizeof(*opts));
    else
        epicsThreadPoolConfigDefaults(&pool->conf);

    if (pool->conf.initialThreads > pool->conf.maxThreads)
        pool->conf.initialThreads = pool->conf.maxThreads;

    pool->workerWakeup = epicsEventCreate(epicsEventEmpty);
    pool->shutdownEvent = epicsEventCreate(epicsEventEmpty);
    pool->observerWakeup = epicsEventCreate(epicsEventEmpty);
    pool->guard = epicsMutexCreate();

    if (!pool->workerWakeup || !pool->shutdownEvent ||
       !pool->observerWakeup || !pool->guard)
        goto cleanup;

    ellInit(&pool->jobs);
    ellInit(&pool->owned);

    epicsMutexMustLock(pool->guard);

    for (i = 0; i < pool->conf.initialThreads; i++) {
        createPoolThread(pool);
    }

    if (pool->threadsRunning == 0 && pool->conf.initialThreads != 0) {
        epicsMutexUnlock(pool->guard);
        errlogPrintf("Error: Unable to create any threads for thread pool\n");
        goto cleanup;

    }
    else if (pool->threadsRunning < pool->conf.initialThreads) {
        errlogPrintf("Warning: Unable to create all threads for thread pool (%u/%u)\n",
                     pool->threadsRunning, pool->conf.initialThreads);
    }

    epicsMutexUnlock(pool->guard);

    return pool;

cleanup:
    if (pool->workerWakeup)
        epicsEventDestroy(pool->workerWakeup);
    if (pool->shutdownEvent)
        epicsEventDestroy(pool->shutdownEvent);
    if (pool->observerWakeup)
        epicsEventDestroy(pool->observerWakeup);
    if (pool->guard)
        epicsMutexDestroy(pool->guard);

    free(pool);
    return NULL;
}

static
void epicsThreadPoolControlImpl(epicsThreadPool *pool, epicsThreadPoolOption opt, unsigned int val)
{
    if (pool->freezeopt)
        return;

    if (opt == epicsThreadPoolQueueAdd) {
        pool->pauseadd = !val;
    }
    else if (opt == epicsThreadPoolQueueRun) {
        if (!val && !pool->pauserun)
            pool->pauserun = 1;

        else if (val && pool->pauserun) {
            int jobs = ellCount(&pool->jobs);
            pool->pauserun = 0;

            if (jobs) {
                int wakeable = pool->threadsSleeping - pool->threadsWaking;

                /* first try to give jobs to sleeping workers */
                if (wakeable) {
                    int wakeup = jobs > wakeable ? wakeable : jobs;
                    assert(wakeup > 0);
                    jobs -= wakeup;
                    pool->threadsWaking += wakeup;
                    epicsEventSignal(pool->workerWakeup);
                    CHECKCOUNT(pool);
                }
            }
            while (jobs-- && pool->threadsRunning < pool->conf.maxThreads) {
                if (createPoolThread(pool) == 0) {
                    pool->threadsWaking++;
                    epicsEventSignal(pool->workerWakeup);
                }
                else
                    break; /* oops, couldn't create worker */
            }
            CHECKCOUNT(pool);
        }
    }
    /* unknown options ignored */

}

void epicsThreadPoolControl(epicsThreadPool *pool, epicsThreadPoolOption opt, unsigned int val)
{
    epicsMutexMustLock(pool->guard);
    epicsThreadPoolControlImpl(pool, opt, val);
    epicsMutexUnlock(pool->guard);
}

int epicsThreadPoolWait(epicsThreadPool *pool, double timeout)
{
    int ret = 0;
    epicsMutexMustLock(pool->guard);

    while (ellCount(&pool->jobs) > 0 || pool->threadsAreAwake > 0) {
        pool->observerCount++;
        epicsMutexUnlock(pool->guard);

        if (timeout < 0.0) {
            epicsEventMustWait(pool->observerWakeup);
        }
        else {
            switch (epicsEventWaitWithTimeout(pool->observerWakeup, timeout)) {
            case epicsEventWaitError:
                cantProceed("epicsThreadPoolWait: failed to wait for Event");
                break;
            case epicsEventWaitTimeout:
                ret = S_pool_timeout;
                break;
            case epicsEventWaitOK:
                ret = 0;
                break;
            }
        }

        epicsMutexMustLock(pool->guard);
        pool->observerCount--;

        if (pool->observerCount)
            epicsEventSignal(pool->observerWakeup);

        if (ret != 0)
            break;
    }

    epicsMutexUnlock(pool->guard);
    return ret;
}

void epicsThreadPoolDestroy(epicsThreadPool *pool)
{
    unsigned int nThr;
    ELLLIST notify;
    ELLNODE *cur;

    if (!pool)
        return;

    ellInit(&notify);

    epicsMutexMustLock(pool->guard);

    /* run remaining queued jobs */
    epicsThreadPoolControlImpl(pool, epicsThreadPoolQueueAdd, 0);
    epicsThreadPoolControlImpl(pool, epicsThreadPoolQueueRun, 1);
    nThr = pool->threadsRunning;
    pool->freezeopt = 1;

    epicsMutexUnlock(pool->guard);

    epicsThreadPoolWait(pool, -1.0);
    /* At this point all queued jobs have run */

    epicsMutexMustLock(pool->guard);

    pool->shutdown = 1;
    /* wakeup all */
    if (pool->threadsWaking < pool->threadsSleeping) {
        pool->threadsWaking = pool->threadsSleeping;
        epicsEventSignal(pool->workerWakeup);
    }

    ellConcat(&notify, &pool->owned);
    ellConcat(&notify, &pool->jobs);

    epicsMutexUnlock(pool->guard);

    if (nThr && epicsEventWait(pool->shutdownEvent) != epicsEventWaitOK){
        errlogMessage("epicsThreadPoolDestroy: wait error");
        return;
    }

    /* all workers are now shutdown */

    /* notify remaining jobs that pool is being destroyed */
    while ((cur = ellGet(&notify)) != NULL) {
        epicsJob *job = CONTAINER(cur, epicsJob, jobnode);

        job->running = 1;
        job->func(job->arg, epicsJobModeCleanup);
        job->running = 0;
        if (job->freewhendone)
            free(job);
        else
            job->pool = NULL; /* orphan */
    }

    epicsEventDestroy(pool->workerWakeup);
    epicsEventDestroy(pool->shutdownEvent);
    epicsEventDestroy(pool->observerWakeup);
    epicsMutexDestroy(pool->guard);

    free(pool);
}


void epicsThreadPoolReport(epicsThreadPool *pool, FILE *fd)
{
    ELLNODE *cur;
    epicsMutexMustLock(pool->guard);

    fprintf(fd, "Thread Pool with %u/%u threads\n"
            " running %d jobs with %u threads\n",
            pool->threadsRunning,
            pool->conf.maxThreads,
            ellCount(&pool->jobs),
            pool->threadsAreAwake);
    if (pool->pauseadd)
        fprintf(fd, "  Inhibit queueing\n");
    if (pool->pauserun)
        fprintf(fd, "  Pause workers\n");
    if (pool->shutdown)
        fprintf(fd, "  Shutdown in progress\n");

    for (cur = ellFirst(&pool->jobs); cur; cur = ellNext(cur)) {
        epicsJob *job = CONTAINER(cur, epicsJob, jobnode);

        fprintf(fd, "  job %p func: %p, arg: %p ",
                job, job->func,
                job->arg);
        if (job->queued)
            fprintf(fd, "Queued ");
        if (job->running)
            fprintf(fd, "Running ");
        if (job->freewhendone)
            fprintf(fd, "Free ");
        fprintf(fd, "\n");
    }

    epicsMutexUnlock(pool->guard);
}

unsigned int epicsThreadPoolNThreads(epicsThreadPool *pool)
{
    unsigned int ret;

    epicsMutexMustLock(pool->guard);
    ret = pool->threadsRunning;
    epicsMutexUnlock(pool->guard);

    return ret;
}

static
ELLLIST sharedPools = ELLLIST_INIT;

static
epicsMutexId sharedPoolsGuard;

static
epicsThreadOnceId sharedPoolsOnce = EPICS_THREAD_ONCE_INIT;

static
void sharedPoolsInit(void* unused)
{
    sharedPoolsGuard = epicsMutexMustCreate();
}

epicsShareFunc epicsThreadPool* epicsThreadPoolGetShared(epicsThreadPoolConfig *opts)
{
    ELLNODE *node;
    epicsThreadPool *cur;
    epicsThreadPoolConfig defopts;
    size_t N = epicsThreadGetCPUs();

    if (!opts) {
        epicsThreadPoolConfigDefaults(&defopts);
        opts = &defopts;
    }
    /* shared pools must have a minimum allowed number of workers.
     * Use the number of CPU cores
     */
    if (opts->maxThreads < N)
        opts->maxThreads = N;

    epicsThreadOnce(&sharedPoolsOnce, &sharedPoolsInit, NULL);

    epicsMutexMustLock(sharedPoolsGuard);

    for (node = ellFirst(&sharedPools); node; node = ellNext(node)) {
        cur = CONTAINER(node, epicsThreadPool, sharedNode);

        /* Must have exactly the requested priority
         * At least the requested max workers
         * and at least the requested stack size
         */
        if (cur->conf.workerPriority != opts->workerPriority)
            continue;
        if (cur->conf.maxThreads < opts->maxThreads)
            continue;
        if (cur->conf.workerStack < opts->workerStack)
            continue;

        cur->sharedCount++;
        assert(cur->sharedCount > 0);
        epicsMutexUnlock(sharedPoolsGuard);

        epicsMutexMustLock(cur->guard);
        *opts = cur->conf;
        epicsMutexUnlock(cur->guard);
        return cur;
    }

    cur = epicsThreadPoolCreate(opts);
    if (!cur) {
        epicsMutexUnlock(sharedPoolsGuard);
        return NULL;
    }
    cur->sharedCount = 1;

    ellAdd(&sharedPools, &cur->sharedNode);
    epicsMutexUnlock(sharedPoolsGuard);
    return cur;
}

epicsShareFunc void epicsThreadPoolReleaseShared(epicsThreadPool *pool)
{
    if (!pool)
        return;

    epicsMutexMustLock(sharedPoolsGuard);

    assert(pool->sharedCount > 0);

    pool->sharedCount--;

    if (pool->sharedCount == 0) {
        ellDelete(&sharedPools, &pool->sharedNode);
        epicsThreadPoolDestroy(pool);
    }

    epicsMutexUnlock(sharedPoolsGuard);
}
