/* osi/os/posix/osdMutex.c */

/* Author:  Marty Kraimer Date:    13AUG1999 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "epicsMutex.h"
#include "cantProceed.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsAssert.h"

/* Until these can be demonstrated to work leave them undefined*/
#undef _POSIX_THREAD_PROCESS_SHARED
#undef _POSIX_THREAD_PRIO_INHERIT

typedef struct epicsMutexOSD {
    pthread_mutexattr_t mutexAttr;
    pthread_mutex_t	lock;
    pthread_cond_t	waitToBeOwner;
#if defined _POSIX_THREAD_PROCESS_SHARED
    pthread_condattr_t  condAttr;
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    int			count;
    int			owned;  /* TRUE | FALSE */
    pthread_t		ownerTid;
}epicsMutexOSD;

#define checkStatus(status,message) \
if((status)) { \
    errlogPrintf("%s failed: error %s\n",(message),strerror((status)));}

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("%s failed: error %s\n",(message),strerror((status))); \
    cantProceed((method)); \
}

epicsMutexId epicsMutexOsdCreate(void) {
    epicsMutexOSD *pmutex;
    int           status;

    pmutex = callocMustSucceed(1,sizeof(*pmutex),"epicsMutexOsdCreate");
    status = pthread_mutexattr_init(&pmutex->mutexAttr);
    checkStatusQuit(status,"pthread_mutexattr_init","epicsMutexOsdCreate");
#if defined _POSIX_THREAD_PRIO_INHERIT
    status = pthread_mutexattr_setprotocol(
        &pmutex->mutexAttr,PTHREAD_PRIO_INHERIT);
    if(errVerbose) checkStatus(status,"pthread_mutexattr_setprotocal");
#endif /*_POSIX_THREAD_PRIO_INHERIT*/
    status = pthread_mutex_init(&pmutex->lock,&pmutex->mutexAttr);
    checkStatusQuit(status,"pthread_mutex_init","epicsMutexOsdCreate");
#if defined _POSIX_THREAD_PROCESS_SHARED
    status = pthread_condattr_init(&pmutex->condAttr);
    checkStatus(status,"pthread_condattr_init");
    status = pthread_condattr_setpshared(&pmutex->condAttr,
        PTHREAD_PROCESS_PRIVATE);
    checkStatus(status,"pthread_condattr_setpshared");
    status = pthread_cond_init(&pmutex->waitToBeOwner,&pmutex->condAttr);
#else
    status = pthread_cond_init(&pmutex->waitToBeOwner,0);
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    checkStatusQuit(status,"pthread_cond_init","epicsMutexOsdCreate");
    return((epicsMutexId)pmutex);
}

void epicsMutexOsdDestroy(epicsMutexId pmutex)
{
    int   status;

    status = pthread_cond_destroy(&pmutex->waitToBeOwner);
    checkStatus(status,"pthread_cond_destroy");
#if defined _POSIX_THREAD_PROCESS_SHARED
    status = pthread_condattr_destroy(&pmutex->condAttr);
#endif /*_POSIX_THREAD_PROCESS_SHARED*/
    status = pthread_mutex_destroy(&pmutex->lock);
    checkStatus(status,"pthread_mutex_destroy");
    status = pthread_mutexattr_destroy(&pmutex->mutexAttr);
    checkStatus(status,"pthread_mutexattr_destroy");
    free(pmutex);
}

void epicsMutexUnlock(epicsMutexId pmutex)
{
    int status;

    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsMutexUnlock");
    if((pmutex->count<=0) || (pmutex->ownerTid != pthread_self())) {
        errlogPrintf("epicsMutexUnlock but caller is not owner\n");
        status = pthread_mutex_unlock(&pmutex->lock);
        checkStatusQuit(status,"pthread_mutex_unlock","epicsMutexUnlock");
        return;
    }
    pmutex->count--;
    if(pmutex->count == 0) {
        pmutex->owned = 0;
        pmutex->ownerTid = 0;
        pthread_cond_signal(&pmutex->waitToBeOwner);
    }
    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsMutexUnlock");
}

epicsMutexLockStatus epicsMutexLock(epicsMutexId pmutex)
{
    pthread_t tid = pthread_self();
    int status;

    if(!pmutex || !tid) return(epicsMutexLockError);
    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsMutexLock");
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid))
        pthread_cond_wait(&pmutex->waitToBeOwner,&pmutex->lock);
    pmutex->ownerTid = tid;
    pmutex->owned = 1;
    pmutex->count++;
    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsMutexLock");
    return(epicsMutexLockOK);
}

epicsMutexLockStatus epicsMutexLockWithTimeout(epicsMutexId pmutex, double timeout)
{
    pthread_t tid = pthread_self();
    struct timespec wakeTime;
    int status,unlockStatus;

    status = convertDoubleToWakeTime(timeout,&wakeTime);
    checkStatusQuit(status,"convertDoubleToWakeTime",
        "epicsMutexLockWithTimeout");
    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsMutexLockWithTimeout");
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid)) {
        status = pthread_cond_timedwait(
            &pmutex->waitToBeOwner,&pmutex->lock,&wakeTime);
        if(!status) break;
    }
    if(status==0) {
        pmutex->ownerTid = tid;
        pmutex->owned = 1;
        pmutex->count++;
    }
    unlockStatus = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(unlockStatus,"pthread_mutex_lock","epicsMutexLockWithTimeout");
    if(status==0) return(epicsMutexLockOK);
    if(status==ETIMEDOUT) return(epicsMutexLockTimeout);
    checkStatusQuit(status,"pthread_cond_timedwait","epicsMutexLockWithTimeout");
    return(epicsMutexLockError);
}

epicsMutexLockStatus epicsMutexTryLock(epicsMutexId pmutex)
{
    pthread_t tid = pthread_self();
    epicsMutexLockStatus status;
    int pthreadStatus;

    pthreadStatus = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(pthreadStatus,"pthread_mutex_lock","epicsMutexTryLock");
    if(!pmutex->owned || pthread_equal(pmutex->ownerTid,tid)) {
        pmutex->ownerTid = tid;
        pmutex->owned = 1;
        pmutex->count++;
        status = epicsMutexLockOK;
    }
    else {
        status = epicsMutexLockTimeout;
    }
    pthreadStatus = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(pthreadStatus,"pthread_mutex_unlock","epicsMutexTryLock");
    return(status);
}

void epicsMutexShow(epicsMutexId pmutex,unsigned int level)
{
    printf("ownerTid %p count %d owned %d\n",
        (void *)pmutex->ownerTid,pmutex->count,pmutex->owned);
}
