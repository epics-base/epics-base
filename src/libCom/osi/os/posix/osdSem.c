/* osiSem.c */

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

#include "osiSem.h"
#include "osiThread.h"
#include "cantProceed.h"
#include "tsStamp.h"
#include "errlog.h"

typedef struct binary {
    pthread_mutex_t	mutex;
    pthread_cond_t	cond;
    int                 isFull;
}binary;

typedef struct mutex {
    pthread_mutexattr_t mutexAttr;
    pthread_mutex_t	lock;
    pthread_cond_t	waitToBeOwner;
    int			count;
    int			owned;  /* TRUE | FALSE */
    pthread_t		ownerTid;
}mutex;

#define checkStatus(status,message) \
if((status)) { \
    errlogPrintf("%s failed: error %s\n",(message),strerror((status)));}

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("%s failed: error %s\n",(message),strerror((status))); \
    cantProceed((method)); \
}

static void convertDoubleToWakeTime(double timeout,struct timespec *wakeTime)
{
    struct timespec wait;
    TS_STAMP stamp;

    if(timeout<0.0) timeout = 0.0;
    else if(timeout>3600.0) timeout = 3600.0;
    tsStampGetCurrent(&stamp);
    tsStampToTimespec(wakeTime, &stamp);
    wait.tv_sec = timeout;
    wait.tv_nsec = (long)((timeout - (double)wait.tv_sec) * 1e9);
    wakeTime->tv_sec += wait.tv_sec;
    wakeTime->tv_nsec += wait.tv_nsec;
    if(wakeTime->tv_nsec>1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}

semBinaryId semBinaryCreate(semInitialState initialState)
{
    binary *pbinary;
    int   status;

    pbinary = callocMustSucceed(1,sizeof(binary),"semBinaryCreate");
    status = pthread_mutex_init(&pbinary->mutex,0);
    checkStatusQuit(status,"pthread_mutex_init","semBinaryCreate");
    status = pthread_cond_init(&pbinary->cond,0);
    checkStatusQuit(status,"pthread_cond_init","semBinaryCreate");
    if(initialState==semFull) pbinary->isFull = 1;
    return((semBinaryId)pbinary);
}

semBinaryId semBinaryMustCreate(semInitialState initialState)
{
    semBinaryId id = semBinaryCreate (initialState);
    assert (id);
    return id;
}

void semBinaryDestroy(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_destroy(&pbinary->mutex);
    checkStatus(status,"pthread_mutex_destroy");
    status = pthread_cond_destroy(&pbinary->cond);
    checkStatus(status,"pthread_cond_destroy");
    free(pbinary);
}

void semBinaryGive(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_lock(&pbinary->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","semBinaryGive");
    if(!pbinary->isFull) {
        pbinary->isFull = 1;
        status = pthread_cond_signal(&pbinary->cond);
        checkStatus(status,"pthread_cond_signal");
    }
    status = pthread_mutex_unlock(&pbinary->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","semBinaryGive");
}

semTakeStatus semBinaryTake(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_lock(&pbinary->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","semBinaryTake");
    if(!pbinary->isFull) {
        status = pthread_cond_wait(&pbinary->cond,&pbinary->mutex);
        checkStatusQuit(status,"pthread_cond_wait","semBinaryTake");
    }
    pbinary->isFull = 0;
    status = pthread_mutex_unlock(&pbinary->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","semBinaryTake");
    return(semTakeOK);
}

semTakeStatus semBinaryTakeTimeout(semBinaryId id, double timeout)
{
    binary *pbinary = (binary *)id;
    struct timespec wakeTime;
    int   status = 0;
    int   unlockStatus;

    status = pthread_mutex_lock(&pbinary->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","semBinaryTakeTimeout");
    if(!pbinary->isFull) {
        convertDoubleToWakeTime(timeout,&wakeTime);
        status = pthread_cond_timedwait(
            &pbinary->cond,&pbinary->mutex,&wakeTime);
    }
    if(status==0) pbinary->isFull = 0;
    unlockStatus = pthread_mutex_unlock(&pbinary->mutex);
    checkStatusQuit(unlockStatus,"pthread_mutex_unlock","semBinaryTakeTimeout");
    if(status==0) return(semTakeOK);
    if(status==ETIMEDOUT) return(semTakeTimeout);
    checkStatus(status,"pthread_cond_timedwait");
    return(semTakeError);
}

semTakeStatus semBinaryTakeNoWait(semBinaryId id)
{
    return(semBinaryTakeTimeout(id,0.0));
}

void semBinaryShow(semBinaryId id,unsigned int level)
{
}

semMutexId semMutexCreate(void) {
    mutex *pmutex;
    int   status;

    pmutex = callocMustSucceed(1,sizeof(mutex),"semMutexCreate");
    status = pthread_mutexattr_init(&pmutex->mutexAttr);
    checkStatusQuit(status,"pthread_mutexattr_init","semMutexCreate");
#ifdef _POSIX_THREAD_PRIO_INHERIT
    status = pthread_mutexattr_setprotocol(
        &pmutex->mutexAttr,PTHREAD_PRIO_INHERIT);
    if(errVerbose) checkStatus(status,"pthread_mutexattr_setprotocal");
#endif
    status = pthread_mutex_init(&pmutex->lock,&pmutex->mutexAttr);
    checkStatusQuit(status,"pthread_mutex_init","semMutexCreate");
    status = pthread_cond_init(&pmutex->waitToBeOwner,0);
    checkStatusQuit(status,"pthread_cond_init","semMutexCreate");
    return((semMutexId)pmutex);
}

semMutexId semMutexMustCreate(void)
{
    semMutexId id = semMutexCreate ();
    assert (id);
    return id;
}

void semMutexDestroy(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    int   status;

    status = pthread_cond_destroy(&pmutex->waitToBeOwner);
    checkStatus(status,"pthread_cond_destroy");
    status = pthread_mutex_destroy(&pmutex->lock);
    checkStatus(status,"pthread_mutex_destroy");
    status = pthread_mutexattr_destroy(&pmutex->mutexAttr);
    checkStatus(status,"pthread_mutexattr_destroy");
    free(pmutex);
}

void semMutexGive(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    int status,unlockStatus;

    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","semMutexGive");
    if((pmutex->count<=0) || (pmutex->ownerTid != pthread_self())) {
        errlogPrintf("semMutexGive but caller is not owner\n");
        status = pthread_mutex_unlock(&pmutex->lock);
        checkStatusQuit(status,"pthread_mutex_unlock","semMutexGive");
        return;
    }
    pmutex->count--;
    if(pmutex->count == 0) {
        pmutex->owned = 0;
        pmutex->ownerTid = 0;
        pthread_cond_signal(&pmutex->waitToBeOwner);
    }
    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_unlock","semMutexGive");
}

semTakeStatus semMutexTake(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    int status;

    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","semMutexTake");
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid))
        pthread_cond_wait(&pmutex->waitToBeOwner,&pmutex->lock);
    pmutex->ownerTid = tid;
    pmutex->owned = 1;
    pmutex->count++;
    status = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_unlock","semMutexTake");
    return(semTakeOK);
}

semTakeStatus semMutexTakeTimeout(semMutexId id, double timeout)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    struct timespec wakeTime;
    int status,unlockStatus;

    convertDoubleToWakeTime(timeout,&wakeTime);
    status = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(status,"pthread_mutex_lock","semMutexTakeTimeout");
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
    checkStatusQuit(unlockStatus,"pthread_mutex_lock","semMutexTakeTimeout");
    if(status==0) return(semTakeOK);
    if(status==ETIMEDOUT) return(semTakeTimeout);
    checkStatusQuit(status,"pthread_cond_timedwait","semMutexTakeTimeout");
    return(semTakeError);
}

semTakeStatus semMutexTakeNoWait(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    semTakeStatus status = semTakeError;
    int pthreadStatus;

    pthreadStatus = pthread_mutex_lock(&pmutex->lock);
    checkStatusQuit(pthreadStatus,"pthread_mutex_lock","semMutexTakeNoWait");
    if(!pmutex->owned || pthread_equal(pmutex->ownerTid,tid)) {
        pmutex->ownerTid = tid;
        pmutex->owned = 1;
        pmutex->count++;
        status = 0;
    }
    pthreadStatus = pthread_mutex_unlock(&pmutex->lock);
    checkStatusQuit(pthreadStatus,"pthread_mutex_unlock","semMutexTakeNoWait");
    return(status);
}

void semMutexShow(semMutexId id,unsigned int level)
{
    mutex *pmutex = (mutex *)id;
    printf("ownerTid %p count %d owned %d\n",
        pmutex->ownerTid,pmutex->count,pmutex->owned);
}
