/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/posix/osdEvent.c */

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
#include "epicsEvent.h"
#include "cantProceed.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsAssert.h"

/* Until these can be demonstrated to work leave them undefined*/
#undef _POSIX_THREAD_PROCESS_SHARED
#undef _POSIX_THREAD_PRIO_INHERIT

typedef struct epicsEventOSD {
    pthread_mutex_t     mutex;
    pthread_cond_t	cond;
    int                 isFull;
}epicsEventOSD;

#define checkStatus(status,message) \
if((status)) { \
    errlogPrintf("epicsEvent %s failed: error %s\n",(message),strerror((status)));}

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("epicsEvent %s failed: error %s\n",(message),strerror((status))); \
    cantProceed((method)); \
}

static int mutexLock(pthread_mutex_t *id)
{
    int status;

    while(1) {
        status = pthread_mutex_lock(id);
        if(status!=EINTR) return status;
        errlogPrintf("pthread_mutex_lock returned EINTR. Violates SUSv3\n");
    }
}

static int condTimedwait(pthread_cond_t *condId, pthread_mutex_t *mutexId,
    struct timespec *time)
{
    int status;
    while(1) {
        status = pthread_cond_timedwait(condId,mutexId,time);
        if(status!=EINTR) return status;
        errlogPrintf("pthread_cond_timedwait returned EINTR. Violates SUSv3\n");
    }
}

static int condWait(pthread_cond_t *condId, pthread_mutex_t *mutexId)
{
    int status;
    while(1) {
        status = pthread_cond_wait(condId,mutexId);
        if(status!=EINTR) return status;
        errlogPrintf("pthread_cond_wait returned EINTR. Violates SUSv3\n");
    }
}

epicsShareFunc epicsEventId epicsShareAPI epicsEventCreate(epicsEventInitialState initialState)
{
    epicsEventOSD *pevent;
    int           status;

    pevent = callocMustSucceed(1,sizeof(*pevent),"epicsEventCreate");
    status = pthread_mutex_init(&pevent->mutex,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsEventCreate");
    status = pthread_cond_init(&pevent->cond,0);
    checkStatusQuit(status,"pthread_cond_init","epicsEventCreate");
    if(initialState==epicsEventFull) pevent->isFull = 1;
    return((epicsEventId)pevent);
}

epicsShareFunc epicsEventId epicsShareAPI epicsEventMustCreate(epicsEventInitialState initialState)
{
    epicsEventId id = epicsEventCreate (initialState);
    assert (id);
    return id;
}

epicsShareFunc void epicsShareAPI epicsEventDestroy(epicsEventId pevent)
{
    int   status;

    status = pthread_mutex_destroy(&pevent->mutex);
    checkStatus(status,"pthread_mutex_destroy");
    status = pthread_cond_destroy(&pevent->cond);
    checkStatus(status,"pthread_cond_destroy");
    free(pevent);
}

epicsShareFunc void epicsShareAPI epicsEventSignal(epicsEventId pevent)
{
    int   status;

    status = mutexLock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventSignal");
    if(!pevent->isFull) {
        pevent->isFull = 1;
        status = pthread_cond_signal(&pevent->cond);
        checkStatus(status,"pthread_cond_signal");
    }
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsEventSignal");
}

epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWait(epicsEventId pevent)
{
    int   status;

    if(!pevent) return(epicsEventWaitError);
    status = mutexLock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventWait");
    /*no need for while since caller must be prepared for no work*/
    if(!pevent->isFull) {
        status = condWait(&pevent->cond,&pevent->mutex);
        checkStatusQuit(status,"pthread_cond_wait","epicsEventWait");
    }
    pevent->isFull = 0;
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsEventWait");
    return(epicsEventWaitOK);
}

epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWaitWithTimeout(epicsEventId pevent, double timeout)
{
    struct timespec wakeTime;
    int   status = 0;
    int   unlockStatus;

    status = mutexLock(&pevent->mutex);
    checkStatusQuit(status,"pthread_mutex_lock","epicsEventWaitWithTimeout");
    if(!pevent->isFull) {
        convertDoubleToWakeTime(timeout,&wakeTime);
        status = condTimedwait(
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

epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventTryWait(epicsEventId id)
{
    return(epicsEventWaitWithTimeout(id,0.0));
}

epicsShareFunc void epicsShareAPI epicsEventShow(epicsEventId id,unsigned int level)
{
}
