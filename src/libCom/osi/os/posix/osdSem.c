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
    pthread_mutexattr_t attr;
    pthread_mutex_t	mutex;
}binary;


typedef struct mutex {
    pthread_mutexattr_t attr;
    pthread_mutex_t	lock;	/*lock for structure */
    pthread_cond_t	waitToBeOwner;
    int			count;
    int			owned;  /* TRUE | FALSE */
    pthread_t		ownerTid;
}mutex;

semBinaryId semBinaryCreate(int initialState)
{
    binary *pbinary;
    int   status;

    pbinary = callocMustSucceed(1,sizeof(binary),"semBinaryCreate");
    status = pthread_mutexattr_init(&pbinary->attr);
    if(status) {
         errlogPrintf("pthread_mutexattr_init failed: error %s\n",
             strerror(status));
         cantProceed("semBinaryCreate");
    }
#if defined _POSIX_THREAD_PRIO_PROTECT
    status = pthread_mutexattr_setprotocol(
        &pbinary->attr,PTHREAD_PROCESS_PRIVATE);
    if(status) {
         errlogPrintf("semBinaryCreate pthread_mutexattr_setprotocal "
             "failed: error %s\n",
             strerror(status));
    }
#endif
    status = pthread_mutex_init(&pbinary->mutex,&pbinary->attr);
    if(status) {
         errlogPrintf("pthread_mutex_init failed: error %s\n",
             strerror(status));
    }
    status = pthread_mutex_init(&pbinary->mutex,&pbinary->attr);
    if(status) {
         errlogPrintf("pthread_mutex_init failed: error %s\n",
             strerror(status));
         cantProceed("semBinaryCreate");
    }
    if(initialState==semEmpty) semBinaryTakeNoWait((semBinaryId)pbinary);
    return((semBinaryId)pbinary);
}

void semBinaryDestroy(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_destroy(&pbinary->mutex);
    if(status) errlogPrintf("pthread_mutex_destroy error %s\n",strerror(status));
    status = pthread_mutexattr_destroy(&pbinary->attr);
    if(status) errlogPrintf("pthread_mutexattr_destroy error %s\n",
        strerror(status));
    free(pbinary);
}

void semBinaryGive(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_unlock(&pbinary->mutex);
    if(status) errlogPrintf("pthread_mutex_unlock error %s\n",strerror(status));
}

semTakeStatus semBinaryTake(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_lock(&pbinary->mutex);
    if(status) errlogPrintf("pthread_mutex_lock error %s\n",strerror(status));
    if(status) return(semTakeError);
    return(semTakeOK);
}

semTakeStatus semBinaryTakeTimeout(semBinaryId id, double timeOut)
{
    binary *pbinary = (binary *)id;
    int   status;
    double waitSoFar=0.0;

    while(1) {
        status = pthread_mutex_trylock(&pbinary->mutex);
        if(!status) return(semTakeOK);
        if(status!=EBUSY) {
            errlogPrintf("pthread_mutex_lock error %s\n",strerror(status));
            return(semTakeError);
        }
        threadSleep(1.0);
        waitSoFar += 1.0;
        if(waitSoFar>=timeOut) break;
    }
    return(semTakeTimeout);
}

semTakeStatus semBinaryTakeNoWait(semBinaryId id)
{
    binary *pbinary = (binary *)id;
    int   status;

    status = pthread_mutex_trylock(&pbinary->mutex);
    if(!status) return(semTakeOK);
    if(status==EBUSY) return(semTakeTimeout);
    errlogPrintf("pthread_mutex_lock error %s\n",strerror(status));
    return(semTakeError);
}

void semBinaryShow(semBinaryId id)
{
}

semMutexId semMutexCreate(void) {
    mutex *pmutex;
    int   status;

    pmutex = callocMustSucceed(1,sizeof(mutex),"semMutexCreate");
    status = pthread_mutexattr_init(&pmutex->attr);
    if(status) {
         errlogPrintf("pthread_mutexattr_init failed: error %s\n",
             strerror(status));
         cantProceed("semMutexCreate");
    }
#ifdef _POSIX_THREAD_PRIO_INHERIT
    status = pthread_mutexattr_setprotocol(&pmutex->attr,PTHREAD_PRIO_INHERIT);
    if(status) {
         errlogPrintf("pthread_mutexattr_setprotocal failed: error %s\n",
             strerror(status));
    }
#endif
    status = pthread_mutex_init(&pmutex->lock,&pmutex->attr);
    if(status) {
         errlogPrintf("pthread_mutex_init failed: error %s\n",
             strerror(status));
         cantProceed("semMutexCreate");
    }
    status = pthread_cond_init(&pmutex->waitToBeOwner,0);
    if(status) {
         errlogPrintf("pthread_cond_init failed: error %s\n",
             strerror(status));
         cantProceed("semMutexCreate");
    }
    return((semMutexId)pmutex);
}

void semMutexDestroy(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    int   status;

    status = pthread_mutex_destroy(&pmutex->lock);
    if(status) errlogPrintf("pthread_mutex_destroy error %s\n",strerror(status));
    status = pthread_cond_destroy(&pmutex->waitToBeOwner);
    if(status) errlogPrintf("pthread_cond_destroy error %s\n",strerror(status));
    status = pthread_mutexattr_destroy(&pmutex->attr);
    if(status) errlogPrintf("pthread_mutexattr_destroy error %s\n",strerror(status));
    free(pmutex);
}

void semMutexGive(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_mutex_lock(&pmutex->lock);
    if(pmutex->count>0) pmutex->count--;
    if(pmutex->count == 0) {
        pmutex->owned = 0;
        pmutex->ownerTid = 0;
        pthread_cond_signal(&pmutex->waitToBeOwner);
    }
    pthread_mutex_unlock(&pmutex->lock);
}
semTakeStatus semMutexTake(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();

    pthread_mutex_lock(&pmutex->lock);
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid))
        pthread_cond_wait(&pmutex->waitToBeOwner,&pmutex->lock);
    pmutex->ownerTid = tid;
    pmutex->owned = 1;
    pmutex->count++;
    pthread_mutex_unlock(&pmutex->lock);
    return(0);
}
semTakeStatus semMutexTakeTimeout(semMutexId id, double timeOut)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    struct timespec wait;
    struct timespec abstime;
    int status;
    TS_STAMP stamp;

    tsStampGetCurrent(&stamp);
    tsStampToTimespec(&abstime, &stamp);
    wait.tv_sec = timeOut;
    wait.tv_nsec = (long)((timeOut - (double)wait.tv_sec) * 1e9);
    abstime.tv_sec += wait.tv_sec;
    abstime.tv_nsec += wait.tv_nsec;
    if(abstime.tv_nsec>1000000000L) {
        abstime.tv_nsec -= 1000000000L;
        ++abstime.tv_sec;
    }
    pthread_mutex_lock(&pmutex->lock);
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid)) {
        int status;
        status = pthread_cond_timedwait(
            &pmutex->waitToBeOwner,&pmutex->lock,&abstime);
        if(!status) break;
        if(status==ETIMEDOUT) return(semTakeTimeout);
        return(semTakeError);
    }
    pmutex->ownerTid = tid;
    pmutex->owned = 1;
    pmutex->count++;
    pthread_mutex_unlock(&pmutex->lock);
    return(0);
}

semTakeStatus semMutexTakeNoWait(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    semTakeStatus status = semTakeError;

    pthread_mutex_lock(&pmutex->lock);
    if(!pmutex->owned || pthread_equal(pmutex->ownerTid,tid)) {
        pmutex->ownerTid = tid;
        pmutex->owned = 1;
        pmutex->count++;
        status = 0;
    }
    pthread_mutex_unlock(&pmutex->lock);
    return(status);
}

void semMutexShow(semMutexId id)
{
    mutex *pmutex = (mutex *)id;
    printf("ownerTid %p count %d owned %d\n",
        pmutex->ownerTid,pmutex->count,pmutex->owned);
}
