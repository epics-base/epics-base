/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* General purpose worker thread pool manager
 * mdavidsaver@bnl.gov
 */
#ifndef EPICSTHREADPOOL_H
#define EPICSTHREADPOOL_H

#include <stdlib.h>
#include <stdio.h>

#include "shareLib.h"
#include "errMdef.h"

#define S_pool_jobBusy   (M_pool| 1) /*Job already queued or running*/
#define S_pool_jobIdle   (M_pool| 2) /*Job was not queued or running*/
#define S_pool_noPool    (M_pool| 3) /*Job not associated with a pool*/
#define S_pool_paused    (M_pool| 4) /*Pool not currently accepting jobs*/
#define S_pool_noThreads (M_pool| 5) /*Can't create worker thread*/
#define S_pool_timeout   (M_pool| 6) /*Pool still busy after timeout*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int initialThreads;
    unsigned int maxThreads;
    unsigned int workerStack;
    unsigned int workerPriority;
} epicsThreadPoolConfig;

typedef struct epicsThreadPool epicsThreadPool;

/* Job function call modes */
typedef enum {
    /* Normal run of job */
    epicsJobModeRun,

    /* Thread pool is being destroyed.
     * A chance to cleanup the job immediately with epicsJobDestroy().
     * If ignored, the job is orphaned (dissociated from the thread pool)
     * and epicsJobDestroy() must be called later.
     */
    epicsJobModeCleanup
} epicsJobMode;

typedef void (*epicsJobFunction)(void* arg, epicsJobMode mode);

typedef struct epicsJob epicsJob;

/* Pool operations */

/* Initialize a pool config with default values.
 * This much be done to preserve future compatibility
 * when new options are added.
 */
epicsShareFunc void epicsThreadPoolConfigDefaults(epicsThreadPoolConfig *);

/* fetch or create a thread pool which can be shared with other users.
 * may return NULL for allocation failures
 */
epicsShareFunc epicsThreadPool* epicsThreadPoolGetShared(epicsThreadPoolConfig *opts);
epicsShareFunc void epicsThreadPoolReleaseShared(epicsThreadPool *pool);

/* If opts is NULL then defaults are used.
 * The opts pointer is not stored by this call, and may exist on the stack.
 */
epicsShareFunc epicsThreadPool* epicsThreadPoolCreate(epicsThreadPoolConfig *opts);

/* Blocks until all worker threads have stopped.
 * Any jobs still attached to this pool receive a callback with EPICSJOB_CLEANUP
 * and are then orphaned.
 */
epicsShareFunc void epicsThreadPoolDestroy(epicsThreadPool *);

/* pool control options */
typedef enum {
    epicsThreadPoolQueueAdd, /* val==0 causes epicsJobQueue to fail, 1 is default */
    epicsThreadPoolQueueRun /* val==0 prevents workers from running jobs, 1 is default */
} epicsThreadPoolOption;

epicsShareFunc void epicsThreadPoolControl(epicsThreadPool* pool,
                                           epicsThreadPoolOption opt,
                                           unsigned int val);

/* Block until job queue is emptied and no jobs are running.
 * Useful after calling epicsThreadPoolControl() with option epicsThreadPoolQueueAdd=0
 *
 * timeout<0 waits forever, timeout==0 polls, timeout>0 waits at most one timeout period
 * Returns 0 for success or non-zero on error (timeout is ETIMEOUT)
 */
epicsShareFunc int epicsThreadPoolWait(epicsThreadPool* pool, double timeout);


/* Per job operations */

/* Special flag for epicsJobCreate().
 * When passed as the third argument "user"
 * the argument passed to the job callback
 * will be the epicsJob*
 */
#define EPICSJOB_SELF epicsJobArgSelfMagic
epicsShareExtern void* epicsJobArgSelfMagic;

/* Creates, but does not add, a new job.
 * If pool is NULL then the job is not associated with any pool and
 * epicsJobMove() must be called before epicsJobQueue().
 * Safe to call from a running job function.
 * Returns a new job pointer, or NULL on error.
 */
epicsShareFunc epicsJob* epicsJobCreate(epicsThreadPool* pool,
                                        epicsJobFunction cb,
                                        void* user);

/* Cancel and free a job structure.  Does not block.
 * Job may not be immediately free'd.
 * Safe to call from a running job function.
 */
epicsShareFunc void epicsJobDestroy(epicsJob*);

/* Move the job to a different pool.
 * If pool is NULL then the job will no longer be associated
 * with any pool.
 * Not thread safe.  Job must not be running or queued.
 * returns 0 on success, non-zero on error.
 */
epicsShareFunc int epicsJobMove(epicsJob* job, epicsThreadPool* pool);

/* Adds the job to the run queue
 * Safe to call from a running job function.
 * returns 0 for success, non-zero on error.
 */
epicsShareFunc int epicsJobQueue(epicsJob*);

/* Remove a job from the run queue if it is queued.
 * Safe to call from a running job function.
 * returns 0 if job was queued and now is not.
 *         1 if job already ran, is running, or was not queued before,
 *         Other non-zero on error
 */
epicsShareFunc int epicsJobUnqueue(epicsJob*);


/* Mostly useful for debugging */

epicsShareFunc void epicsThreadPoolReport(epicsThreadPool *pool, FILE *fd);

/* Current number of active workers.  May be less than the maximum */
epicsShareFunc unsigned int epicsThreadPoolNThreads(epicsThreadPool *);

#ifdef __cplusplus
}
#endif

#endif // EPICSTHREADPOOL_H
