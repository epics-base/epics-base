/*************************************************************************\
* Copyright (c) 2021 Fritz Haber Institute, Berlin
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * RTEMS osdEvent.c
 *      Author: H. Junkes
 *              junkes@fhi.mpg.de
 */

#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <rtems.h>
#include <rtems/error.h>

#include <rtems/thread.h>

#include "libComAPI.h"
#include "epicsEvent.h"

typedef struct epicsEventOSD {
    rtems_binary_semaphore rbs;
} epicsEventOSD;

/*
 * Create a simple binary semaphore
 */
LIBCOM_API epicsEventId
epicsEventCreate(epicsEventInitialState initialState)
{
    epicsEventOSD *pSem = malloc (sizeof(*pSem));

    if (pSem) {
        rtems_binary_semaphore_init(&pSem->rbs, NULL);
        if (initialState)
            rtems_binary_semaphore_post(&pSem->rbs);
    }
    return pSem;
}

LIBCOM_API void
epicsEventDestroy(epicsEventId pSem)
{
    rtems_binary_semaphore_destroy(&pSem->rbs);
}

LIBCOM_API epicsEventStatus
epicsEventTrigger(epicsEventId pSem)
{
    rtems_binary_semaphore_post(&pSem->rbs);
    return epicsEventOK;
}

LIBCOM_API epicsEventStatus
epicsEventWait(epicsEventId pSem)
{
    rtems_binary_semaphore_wait(&pSem->rbs);
    return epicsEventOK;
}

LIBCOM_API epicsEventStatus
epicsEventWaitWithTimeout(epicsEventId pSem, double timeout)
{
    int sc;
    rtems_interval delay;
    rtems_interval rate = rtems_clock_get_ticks_per_second();

    if (!rate)
        return epicsEventError;

    if (timeout <= 0.0) {
        sc = rtems_binary_semaphore_try_wait(&pSem->rbs);
        if (!sc)
            return epicsEventOK;
        else
            return epicsEventWaitTimeout;
    }
    else if (timeout < (double) UINT32_MAX / rate) {
        delay = timeout * rate;
        if (delay == 0) {
            /* 0 < timeout < 1/rate; round up */
            delay = 1;
        }
    }
    else {
        /* timeout is NaN or too big to represent; wait forever */
        delay = RTEMS_NO_TIMEOUT;
    }

    sc = rtems_binary_semaphore_wait_timed_ticks(&pSem->rbs, delay);
    if (!sc)
        return epicsEventOK;
    else if (sc == ETIMEDOUT)
        return epicsEventWaitTimeout;
    else
        return epicsEventError;
}

LIBCOM_API epicsEventStatus
epicsEventTryWait(epicsEventId pSem)
{
    int sc = rtems_binary_semaphore_try_wait(&pSem->rbs);

    if (!sc)
        return epicsEventOK;
    else
        return epicsEventWaitTimeout;
}

LIBCOM_API void
epicsEventShow(epicsEventId pSem, unsigned int level)
{
}
