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
#include "cantProceed.h"
#include "errlog.h"


typedef struct cond {
    pthread_mutex_t	mutex;
    pthread_cond_t	condition;
}cond;


typedef struct mutex {
    pthread_mutexattr_t attr;
    pthread_mutex_t	lock;	/*lock for structure */
    pthread_cond_t	waitToBeOwner;
    int			count;
    int			owned;  /* TRUE | FALSE */
    pthread_t		ownerTid;
}mutex;

semId semBinaryCreate(int initialState)
{
    cond *pcond;
    int   status;

    pcond = callocMustSucceed(1,sizeof(cond),"semBinaryCreate");
    status = pthread_mutex_init(&pcond->mutex,0);
    if(status) {
         errlogPrintf("pthread_mutex_init failed: error %s\n",
             strerror(status));
         cantProceed("semBinaryCreate");
    }
    status = pthread_cond_init(&pcond->condition,0);
    if(status) {
         errlogPrintf("pthread_cond_init failed: error %s\n",
             strerror(status));
         cantProceed("semBinaryCreate");
    }
    return((semId)pcond);
}

void semBinaryDestroy(semId id)
{
    cond *pcond = (cond *)id;
    int   status;

    status = pthread_mutex_destroy(&pcond->mutex);
    if(status) errlogPrintf("pthread_mutex_destroy error %s\n",strerror(status));
    status = pthread_cond_destroy(&pcond->condition);
    if(status) errlogPrintf("pthread_cond_destroy error %s\n",strerror(status));
    free(pcond);
}

void semBinaryGive(semId id)
{
    cond *pcond = (cond *)id;
    int   status;

    status = pthread_cond_signal(&pcond->condition);
    if(status) errlogPrintf("pthread_cond_signal error %s\n",strerror(status));
}

semTakeStatus semBinaryTake(semId id)
{
    cond *pcond = (cond *)id;
    int   status;

    status = pthread_cond_wait(&pcond->condition,&pcond->mutex);
    if(status) errlogPrintf("pthread_cond_wait error %s\n",strerror(status));
    if(status) return(semTakeError);
    return(semTakeOK);
}

semTakeStatus semBinaryTakeTimeout(semId id, double timeOut)
{
    cond *pcond = (cond *)id;
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    struct timespec wait;
    int status;

    wait.tv_sec = timeOut;
    wait.tv_nsec = (long)((timeOut - (double)wait.tv_sec) * 1e9);
    status = pthread_cond_timedwait(
        &pcond->condition,&pcond->mutex,&wait);
    if(status==ETIMEDOUT) return(semTakeTimeout);
    if(status) errlogPrintf("pthread_cond_wait error %s\n",strerror(status));
    return(0);
}

semTakeStatus semBinaryTakeNoWait(semId id)
{
    return(semBinaryTakeTimeout(id,0.0));
}

void semBinaryShow(semId id)
{
}

semId semMutexCreate(void) {
    mutex *pmutex;
    int   status;

    pmutex = callocMustSucceed(1,sizeof(mutex),"semMutexCreate");
    status = pthread_mutexattr_init(&pmutex->attr);
    if(status) {
         errlogPrintf("pthread_mutexattr_init failed: error %s\n",
             strerror(status));
         cantProceed("semMutexCreate");
    }
#ifdef PTHREAD_PRIO_INHERIT
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
    return((semId)pmutex);
}

void semMutexDestroy(semId id)
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

void semMutexGive(semId id)
{
    mutex *pmutex = (mutex *)id;
    pthread_mutex_lock(&pmutex->lock);
    pmutex->count--;
    if(pmutex->count == 0) {
        pmutex->owned = 0;
        pmutex->ownerTid = 0;
        pthread_cond_signal(&pmutex->waitToBeOwner);
    }
    pthread_mutex_unlock(&pmutex->lock);
}
semTakeStatus semMutexTake(semId id)
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
semTakeStatus semMutexTakeTimeout(semId id, double timeOut)
{
    mutex *pmutex = (mutex *)id;
    pthread_t tid = pthread_self();
    struct timespec wait;

    wait.tv_sec = timeOut;
    wait.tv_nsec = (long)((timeOut - (double)wait.tv_sec) * 1e9);

    pthread_mutex_lock(&pmutex->lock);
    while(pmutex->owned && !pthread_equal(pmutex->ownerTid,tid)) {
        int status;
        status = pthread_cond_timedwait(
            &pmutex->waitToBeOwner,&pmutex->lock,&wait);
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
semTakeStatus semMutexTakeNoWait(semId id)
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
void semMutexShow(semId id)
{
    mutex *pmutex = (mutex *)id;
    printf("ownerTid %p count %d owned %d\n",
        pmutex->ownerTid,pmutex->count,pmutex->owned);
}
