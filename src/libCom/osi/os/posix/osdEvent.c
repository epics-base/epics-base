/* osi/os/posix/osdEvent.c */

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

#include "epicsEvent.h"
#include "cantProceed.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsAssert.h"

/* Until these can be demonstrated to work leave them undefined*/
#undef _POSIX_THREAD_PROCESS_SHARED
#undef _POSIX_THREAD_PRIO_INHERIT

typedef struct event {
    pthread_mutex_t     mutex;
    pthread_cond_t	cond;
    int                 isFull;
}event;

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
    epicsTimeStamp stamp;

    if(timeout<0.0) timeout = 0.0;
    else if(timeout>3600.0) timeout = 3600.0;
    epicsTimeGetCurrent(&stamp);
    epicsTimeToTimespec(wakeTime, &stamp);
    wait.tv_sec = timeout;
    wait.tv_nsec = (long)((timeout - (double)wait.tv_sec) * 1e9);
    wakeTime->tv_sec += wait.tv_sec;
    wakeTime->tv_nsec += wait.tv_nsec;
    if(wakeTime->tv_nsec>1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}

epicsEventId epicsEventCreate(epicsEventInitialState initialState)
{
    event *pevent;
    int   status;

    pevent = callocMustSucceed(1,sizeof(event),"epicsEventCreate");
    status = pthread_mutex_init(&pevent->mutex,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsEventCreate");
    status = pthread_cond_init(&pevent->cond,0);
    checkStatusQuit(status,"pthread_cond_init","epicsEventCreate");
    if(initialState==epicsEventFull) pevent->isFull = 1;
    return((epicsEventId)pevent);
}

epicsEventId epicsEventMustCreate(epicsEventInitialState initialState)
{
    epicsEventId id = epicsEventCreate (initialState);
    assert (id);
    return id;
}

void epicsEventDestroy(epicsEventId id)
{
    event *pevent = (event *)id;
    int   status;

    status = pthread_mutex_destroy(&pevent->mutex);
    checkStatus(status,"pthread_mutex_destroy");
    status = pthread_cond_destroy(&pevent->cond);
    checkStatus(status,"pthread_cond_destroy");
    free(pevent);
}

void epicsEventSignal(epicsEventId id)
{
    event *pevent = (event *)id;
    int   status;

    status = pthread_mutex_lock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventSignal");
    if(!pevent->isFull) {
        pevent->isFull = 1;
        status = pthread_cond_signal(&pevent->cond);
        checkStatus(status,"pthread_cond_signal");
    }
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsEventSignal");
}

epicsEventWaitStatus epicsEventWait(epicsEventId id)
{
    event *pevent = (event *)id;
    int   status;

    if(!pevent) return(epicsEventWaitError);
    status = pthread_mutex_lock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventWait");
    /*no need for while since caller must be prepared for no work*/
    if(!pevent->isFull) {
        status = pthread_cond_wait(&pevent->cond,&pevent->mutex);
        checkStatusQuit(status,"pthread_cond_wait","epicsEventWait");
    }
    pevent->isFull = 0;
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsEventWait");
    return(epicsEventWaitOK);
}

epicsEventWaitStatus epicsEventWaitWithTimeout(epicsEventId id, double timeout)
{
    event *pevent = (event *)id;
    struct timespec wakeTime;
    int   status = 0;
    int   unlockStatus;

    status = pthread_mutex_lock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventWaitWithTimeout");
    if(!pevent->isFull) {
        convertDoubleToWakeTime(timeout,&wakeTime);
        status = pthread_cond_timedwait(
            &pevent->cond,&pevent->mutex,&wakeTime);
    }
    if(status==0) pevent->isFull = 0;
    unlockStatus = pthread_mutex_unlock(&pevent->mutex);
    checkStatusQuit(unlockStatus,"pthread_mutex_unlock","epicsEventWaitWithTimeout");
    if(status==0) return(epicsEventWaitOK);
    if(status==ETIMEDOUT) return(epicsEventWaitTimeout);
    checkStatus(status,"pthread_cond_timedwait");
    return(epicsEventWaitError);
}

epicsEventWaitStatus epicsEventTryWait(epicsEventId id)
{
    return(epicsEventWaitWithTimeout(id,0.0));
}

void epicsEventShow(epicsEventId id,unsigned int level)
{
}
