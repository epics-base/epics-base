/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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

#include "epicsEvent.h"
#include "epicsTime.h"
#include "errlog.h"
#include "osdPosixMutexPriv.h"

struct epicsEventOSD {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             isFull;
};

#define printStatus(status, routine, func) \
    errlogPrintf("%s: %s failed: %s\n", (func), (routine), strerror(status))

#define checkStatus(status, routine, func) \
    if (status) { \
        printStatus(status, routine, func); \
    }

#define checkStatusReturn(status, routine, func) \
    if (status) { \
        printStatus(status, routine, func); \
        return epicsEventError; \
    }


LIBCOM_API epicsEventId epicsEventCreate(epicsEventInitialState init)
{
    epicsEventId pevent = malloc(sizeof(*pevent));

    if (pevent) {
        int status = osdPosixMutexInitDefault(&pevent->mutex);

        pevent->isFull = (init == epicsEventFull);
        if (status) {
            printStatus(status, "osdPosixMutexInitDefault", "epicsEventCreate");
        } else {
            status = pthread_cond_init(&pevent->cond, 0);
            if (!status)
                return pevent;
            printStatus(status, "pthread_cond_init", "epicsEventCreate");
            status = pthread_mutex_destroy(&pevent->mutex);
            checkStatus(status, "pthread_mutex_destroy", "epicsEventCreate");
        }
        free(pevent);
    }
    return NULL;
}

LIBCOM_API void epicsEventDestroy(epicsEventId pevent)
{
    int status = pthread_mutex_destroy(&pevent->mutex);

    checkStatus(status, "pthread_mutex_destroy", "epicsEventDestroy");
    status = pthread_cond_destroy(&pevent->cond);
    checkStatus(status, "pthread_cond_destroy", "epicsEventDestroy");
    free(pevent);
}

LIBCOM_API epicsEventStatus epicsEventTrigger(epicsEventId pevent)
{
    int status = pthread_mutex_lock(&pevent->mutex);

    checkStatusReturn(status, "pthread_mutex_lock", "epicsEventTrigger");
    if (!pevent->isFull) {
        pevent->isFull = 1;
        status = pthread_cond_signal(&pevent->cond);
        checkStatus(status, "pthread_cond_signal", "epicsEventTrigger");
    }
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusReturn(status, "pthread_mutex_unlock", "epicsEventTrigger");
    return epicsEventOK;
}

LIBCOM_API epicsEventStatus epicsEventWait(epicsEventId pevent)
{
    epicsEventStatus result = epicsEventOK;
    int status = pthread_mutex_lock(&pevent->mutex);

    checkStatusReturn(status, "pthread_mutex_lock", "epicsEventWait");
    while (!pevent->isFull) {
        status = pthread_cond_wait(&pevent->cond, &pevent->mutex);
        if (status) {
            printStatus(status, "pthread_cond_wait", "epicsEventWait");
            result = epicsEventError;
            goto release;
        }
    }
    pevent->isFull = 0;
    result = epicsEventOK;
release:
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusReturn(status, "pthread_mutex_unlock", "epicsEventWait");
    return result;
}

LIBCOM_API epicsEventStatus epicsEventWaitWithTimeout(epicsEventId pevent,
    double timeout)
{
    epicsEventStatus result = epicsEventOK;
    int status = pthread_mutex_lock(&pevent->mutex);

    checkStatusReturn(status, "pthread_mutex_lock", "epicsEventWaitWithTimeout");
    if (!pevent->isFull) {
        struct timespec wakeTime;

        convertDoubleToWakeTime(timeout, &wakeTime);
        while (!status && !pevent->isFull) {
            status = pthread_cond_timedwait(&pevent->cond, &pevent->mutex,
                &wakeTime);
        }
        if (status) {
            result = (status == ETIMEDOUT) ?
                epicsEventWaitTimeout : epicsEventError;
            goto release;
        }
    }
    pevent->isFull = 0;
release:
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusReturn(status, "pthread_mutex_unlock", "epicsEventWaitWithTimeout");
    return result;
}

LIBCOM_API epicsEventStatus epicsEventWaitWithAbsTimeout(epicsEventId pevent,
    const epicsTimeStamp * abs_timeout)
{
    epicsEventStatus result = epicsEventOK;
    struct timespec  ts;
    int status;

    if ( epicsTimeToTimespec(&ts, abs_timeout) ) {
        /* I don't think this can actually fail */
        return epicsEventError;
    }    

    status = pthread_mutex_lock(&pevent->mutex);

    checkStatusReturn(status, "pthread_mutex_lock", "epicsEventWaitWithAbsTimeout");
    if (!pevent->isFull) {
        while (!status && !pevent->isFull) {
            status = pthread_cond_timedwait(&pevent->cond, &pevent->mutex, &ts);
        }
        if (status) {
            result = (status == ETIMEDOUT) ?
                epicsEventWaitTimeout : epicsEventError;
            goto release;
        }
    }
    pevent->isFull = 0;
release:
    status = pthread_mutex_unlock(&pevent->mutex);
    checkStatusReturn(status, "pthread_mutex_unlock", "epicsEventWaitWithAbsTimeout");
    return result;
}


LIBCOM_API epicsEventStatus epicsEventTryWait(epicsEventId id)
{
    return epicsEventWaitWithTimeout(id, 0.0);
}

LIBCOM_API void epicsEventShow(epicsEventId pevent, unsigned int level)
{
    printf("epicsEvent %p: %s\n", pevent,
        pevent->isFull ? "full" : "empty");
    if (level > 0)
        printf("    pthread_mutex = %p, pthread_cond = %p\n",
            &pevent->mutex, &pevent->cond);
}
