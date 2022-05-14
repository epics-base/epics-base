/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

#include <limits.h>
#include "epicsMessageQueue.h"

extern "C" int sysClkRateGet(void);

LIBCOM_API int epicsStdCall epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout)
{
    int ticks;
    int rate = sysClkRateGet();

    if (timeout <= 0.0) {
        ticks = 0;
    }
    else if (timeout < (double) INT_MAX / rate) {
        ticks = timeout * rate;
        if (ticks == 0) {
            /* 0 < timeout < 1/rate; round up */
            ticks = 1;
        }
    }
    else {
        /* timeout is NaN or too big to represent in ticks */
        ticks = WAIT_FOREVER;
    }

    return msgQSend((MSG_Q_ID)id, (char *)message, size, ticks, MSG_PRI_NORMAL);
}

LIBCOM_API int epicsStdCall epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout)
{
    int ticks;
    int rate = sysClkRateGet();

    if (timeout <= 0.0) {
        ticks = 0;
    }
    else if (timeout < (double) INT_MAX / rate) {
        ticks = timeout * rate;
        if (ticks == 0) {
            /* 0 < timeout < 1/rate, round up */
            ticks = 1;
        }
    }
    else {
        /* timeout is NaN or too big to represent in ticks */
        ticks = WAIT_FOREVER;
    }

    return msgQReceive((MSG_Q_ID)id, (char *)message, size, ticks);
}
