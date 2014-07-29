/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef POOLPRIV_H
#define POOLPRIV_H

#include "epicsThreadPool.h"
#include "ellLib.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMutex.h"

struct epicsThreadPool {
    ELLNODE sharedNode;
    size_t sharedCount;

    ELLLIST jobs; /* run queue */
    ELLLIST owned; /* unqueued jobs. */

    /* Worker state counters.
     * The life cycle of a worker is
     *   Wakeup -> Awake -> Sleeping
     * Newly created workers go into the wakeup state
     */

    /* # of running workers which are not waiting for a wakeup event */
    unsigned int threadsAreAwake;
    /* # of sleeping workers which need to be awakened */
    unsigned int threadsWaking;
    /* # of workers waiting on the workerWakeup event */
    unsigned int threadsSleeping;
    /* # of threads started and not stopped */
    unsigned int threadsRunning;

    /* # of observers waiting on pool events */
    unsigned int observerCount;

    epicsEventId workerWakeup;
    epicsEventId shutdownEvent;

    epicsEventId observerWakeup;

    /* Disallow epicsJobQueue */
    unsigned int pauseadd:1;
    /* Prevent workers from running new jobs */
    unsigned int pauserun:1;
    /* Prevent further changes to pool options */
    unsigned int freezeopt:1;
    /* tell workers to exit */
    unsigned int shutdown:1;

    epicsMutexId guard;

    /* copy of config passed when created */
    epicsThreadPoolConfig conf;
};

/* Called after manipulating counters to check that invariants are preserved */
#define CHECKCOUNT(pPool) do { \
    if (!(pPool)->shutdown) { \
        assert((pPool)->threadsAreAwake + (pPool)->threadsSleeping == (pPool)->threadsRunning); \
        assert((pPool)->threadsWaking <= (pPool)->threadsSleeping); \
    } \
} while(0)

/* When created a job is idle.  queued and running are false
 * and jobnode is in the thread pool's owned list.
 *
 * When the job is added, the queued flag is set and jobnode
 * is in the jobs list.
 *
 * When the job starts running the queued flag is cleared and
 * the running flag is set.  jobnode is not in any list
 * (held locally by worker).
 *
 * When the job has finished running, the running flag is cleared.
 * The queued flag may be set if the job re-added itself.
 * Based on the queued flag jobnode is added to the appropriate
 * list.
 */
struct epicsJob {
    ELLNODE jobnode;
    epicsJobFunction func;
    void *arg;
    epicsThreadPool *pool;

    unsigned int queued:1;
    unsigned int running:1;
    unsigned int freewhendone:1; /* lazy delete of running job */
    unsigned int dead:1; /* flag to catch use of freed objects */
};

int createPoolThread(epicsThreadPool *pool);

#endif // POOLPRIV_H
