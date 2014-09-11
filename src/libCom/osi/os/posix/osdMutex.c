/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/posix/osdMutex.c */

/* Author:  Marty Kraimer Date:    13AUG1999 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "cantProceed.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsAssert.h"

#define checkStatus(status,message) \
    if((status)) { \
        errlogPrintf("epicsMutex %s failed: error %s\n", \
            (message), strerror((status))); \
    }
#define checkStatusQuit(status,message,method) \
    if(status) { \
        errlogPrintf("epicsMutex %s failed: error %s\n", \
            (message), strerror((status))); \
        cantProceed((method)); \
    }

static int mutexLock(pthread_mutex_t *id)
{
    int status;

    while ((status = pthread_mutex_lock(id)) == EINTR) {
        errlogPrintf("pthread_mutex_lock returned EINTR. Violates SUSv3\n");
    }
    return status;
}

/* Until these can be demonstrated to work leave them undefined*/
/* On solaris 8 _POSIX_THREAD_PRIO_INHERIT fails*/
#undef _POSIX_THREAD_PROCESS_SHARED
#undef _POSIX_THREAD_PRIO_INHERIT

/* Two completely different implementations are provided below
 * If support is available for PTHREAD_MUTEX_RECURSIVE then
 *      only pthread_mutex is used.
 * If support is not available for PTHREAD_MUTEX_RECURSIVE then
 *      a much more complicated solution is required
 */


#if defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE)>=500
typedef struct epicsMutexOSD {
    pthread_mutex_t     lock;
    pthread_mutexattr_t mutexAttr;
} epicsMutexOSD;

epicsMutexOSD * epicsMutexOsdCreate(void) {
    epicsMutexOSD *pmutex;
    int status;

    pmutex = calloc(1, sizeof(*pmutex));
    if(!pmutex)
        goto fail;

    status = pthread_mutexattr_init(&pmutex->mutexAttr);
    if (status)
        goto fail;

#if defined(_POSIX_THREAD_PRIO_INHERIT) && _POSIX_THREAD_PRIO_INHERIT > 0
    status = pthread_mutexattr_setprotocol(&pmutex->mutexAttr,
        PTHREAD_PRIO_INHERIT);
    if (errVerbose) checkStatus(status, "pthread_mutexattr_setprotocal");
#endif /*_POSIX_THREAD_PRIO_INHERIT*/

    status = pthread_mutexattr_settype(&pmutex->mutexAttr,
        PTHREAD_MUTEX_RECURSIVE);
    checkStatus(status, "pthread_mutexattr_settype");
    if (status)
        goto fail;

    status = pthread_mutex_init(&pmutex->lock, &pmutex->mutexAttr);
    if (status)
        goto dattr;
    return pmutex;

dattr:
    pthread_mutexattr_destroy(&pmutex->mutexAttr);
fail:
    free(pmutex);
    return NULL;
}

void epicsMutexOsdDestroy(struct epicsMutexOSD * pmutex)
{
    int status;

    status = pthread_mutex_destroy(&pmutex->lock);
    checkStatus(status, "pthread_mutex_destroy");
    status = pthread_mutexattr_destroy(&pmutex->mutexAttr);
    checkStatus(status, "pthread_mutexattr_destroy");
    free(pmutex);
}

void epicsMutexOsdUnlock(struct epicsMutexOSD * pmutex)
{
    int status;

    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatus(status, "pthread_mutex_unlock epicsMutexOsdUnlock");
}

epicsMutexLockStatus epicsMutexOsdLock(struct epicsMutexOSD * pmutex)
{
    int status;

    status = mutexLock(&pmutex->lock);
    if (status == EINVAL) return epicsMutexLockError;
    if(status) {
        errlogMessage("epicsMutex pthread_mutex_lock failed: error epicsMutexOsdLock\n");
        return epicsMutexLockError;
    }
    return epicsMutexLockOK;
}

epicsMutexLockStatus epicsMutexOsdTryLock(struct epicsMutexOSD * pmutex)
{
    int status;

    if (!pmutex) return epicsMutexLockError;
    status = pthread_mutex_trylock(&pmutex->lock);
    if (status == EINVAL) return epicsMutexLockError;
    if (status == EBUSY) return epicsMutexLockTimeout;
    if(status) {
        errlogMessage("epicsMutex pthread_mutex_trylock failed: error epicsMutexOsdTryLock");
        return epicsMutexLockError;
    }
    return epicsMutexLockOK;
}

void epicsMutexOsdShow(struct epicsMutexOSD * pmutex, unsigned int level)
{
}

#else /*defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE)>=500 */

typedef struct epicsMutexOSD {
    pthread_mutex_t	lock;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t	waitToBeOwner;
#if defined(_POSIX_THREAD_PROCESS_SHARED) && _POSIX_THREAD_PROCESS_SHARED > 0
    pthread_condattr_t  condAttr;
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    int			count;
    int			owned;  /* TRUE | FALSE */
    pthread_t		ownerTid;
} epicsMutexOSD;

epicsMutexOSD * epicsMutexOsdCreate(void) {
    epicsMutexOSD *pmutex;
    int           status;

    pmutex = calloc(1, sizeof(*pmutex));
    if(!pmutex)
        return NULL;

    status = pthread_mutexattr_init(&pmutex->mutexAttr);
    if(status)
        goto fail;

#if defined(_POSIX_THREAD_PRIO_INHERIT) && _POSIX_THREAD_PRIO_INHERIT > 0
    status = pthread_mutexattr_setprotocol(
        &pmutex->mutexAttr,PTHREAD_PRIO_INHERIT);
    if (errVerbose) checkStatus(status, "pthread_mutexattr_setprotocal");
#endif /*_POSIX_THREAD_PRIO_INHERIT*/

    status = pthread_mutex_init(&pmutex->lock, &pmutex->mutexAttr);
    if(status)
        goto dattr;

#if defined(_POSIX_THREAD_PROCESS_SHARED) && _POSIX_THREAD_PROCESS_SHARED > 0
    status = pthread_condattr_init(&pmutex->condAttr);
    checkStatus(status, "pthread_condattr_init");
    status = pthread_condattr_setpshared(&pmutex->condAttr,
        PTHREAD_PROCESS_PRIVATE);
    checkStatus(status, "pthread_condattr_setpshared");
    status = pthread_cond_init(&pmutex->waitToBeOwner, &pmutex->condAttr);
#else
    status = pthread_cond_init(&pmutex->waitToBeOwner, 0);
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    if(status)
        goto dmutex;

    return pmutex;

dmutex:
    pthread_mutex_destroy(&pmutex->lock);
dattr:
    pthread_mutexattr_destroy(&pmutex->mutexAttr);
fail:
    free(pmutex);
    return NULL;
}

void epicsMutexOsdDestroy(struct epicsMutexOSD * pmutex)
{
    int   status;

    status = pthread_cond_destroy(&pmutex->waitToBeOwner);
    checkStatus(status, "pthread_cond_destroy");
#if defined(_POSIX_THREAD_PROCESS_SHARED) && _POSIX_THREAD_PROCESS_SHARED > 0
    status = pthread_condattr_destroy(&pmutex->condAttr);
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    status = pthread_mutex_destroy(&pmutex->lock);
    checkStatus(status, "pthread_mutex_destroy");
    status = pthread_mutexattr_destroy(&pmutex->mutexAttr);
    checkStatus(status, "pthread_mutexattr_destroy");
    free(pmutex);
}

void epicsMutexOsdUnlock(struct epicsMutexOSD * pmutex)
{
    int status;

    status = mutexLock(&pmutex->lock);
    checkStatus(status, "pthread_mutex_lock epicsMutexOsdUnlock");
    if(status)
        return;

    if ((pmutex->count <= 0) || (pmutex->ownerTid != pthread_self())) {
        pthread_mutex_unlock(&pmutex->lock);
        checkStatus(status, "pthread_mutex_unlock epicsMutexOsdUnlock");
        errlogPrintf("epicsMutexOsdUnlock but caller is not owner\n");
        cantProceed("epicsMutexOsdUnlock but caller is not owner");
        return;
    }

    pmutex->count--;
    if (pmutex->count == 0) {
        pmutex->owned = 0;
        pmutex->ownerTid = 0;
        status = pthread_cond_signal(&pmutex->waitToBeOwner);
        checkStatusQuit(status, "pthread_cond_signal epicsMutexOsdUnlock", "epicsMutexOsdUnlock");
    }

    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatus(status, "pthread_mutex_unlock epicsMutexOsdUnlock");
}

static int condWait(pthread_cond_t *condId, pthread_mutex_t *mutexId)
{
    int status;

    while ((status = pthread_cond_wait(condId, mutexId)) == EINTR) {
        errlogPrintf("pthread_cond_wait returned EINTR. Violates SUSv3\n");
    }
    return status;
}

epicsMutexLockStatus epicsMutexOsdLock(struct epicsMutexOSD * pmutex)
{
    pthread_t tid = pthread_self();
    int status;

    if (!pmutex || !tid) return epicsMutexLockError;
    status = mutexLock(&pmutex->lock);
    if (status == EINVAL) return epicsMutexLockError;
    checkStatus(status, "pthread_mutex_lock epicsMutexOsdLock");
    if(status)
        return epicsMutexLockError;

    while (pmutex->owned && !pthread_equal(pmutex->ownerTid, tid))
        condWait(&pmutex->waitToBeOwner, &pmutex->lock);
    pmutex->ownerTid = tid;
    pmutex->owned = 1;
    pmutex->count++;

    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatus(status, "pthread_mutex_unlock epicsMutexOsdLock");
    if(status)
        return epicsMutexLockError;
    return epicsMutexLockOK;
}

epicsMutexLockStatus epicsMutexOsdTryLock(struct epicsMutexOSD * pmutex)
{
    pthread_t tid = pthread_self();
    epicsMutexLockStatus result;
    int status;

    status = mutexLock(&pmutex->lock);
    if (status == EINVAL) return epicsMutexLockError;
    checkStatus(status, "pthread_mutex_lock epicsMutexOsdTryLock");
    if(status)
        return epicsMutexLockError;

    if (!pmutex->owned || pthread_equal(pmutex->ownerTid, tid)) {
        pmutex->ownerTid = tid;
        pmutex->owned = 1;
        pmutex->count++;
        result = epicsMutexLockOK;
    } else {
        result = epicsMutexLockTimeout;
    }

    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatus(status, "pthread_mutex_unlock epicsMutexOsdTryLock");
    if(status)
        return epicsMutexLockError;
    return result;
}

void epicsMutexOsdShow(struct epicsMutexOSD *pmutex,unsigned int level)
{
    printf("ownerTid %p count %d owned %d\n",
        (void *)pmutex->ownerTid, pmutex->count, pmutex->owned);
}
#endif /*defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE)>=500 */
