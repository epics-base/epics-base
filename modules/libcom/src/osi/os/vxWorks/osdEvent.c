/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* os/vxWorks/osdEvent.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

#include <vxWorks.h>
#include <semLib.h>
#include <time.h>
#include <objLib.h>
#include <sysLib.h>
#include <limits.h>

#include "epicsEvent.h"

/* The following not defined in any vxWorks header */
int sysClkRateGet(void);

epicsEventId epicsEventCreate(epicsEventInitialState initialState)
{
    return (epicsEventId) semBCreate(SEM_Q_FIFO,
        (initialState == epicsEventEmpty) ? SEM_EMPTY : SEM_FULL);
}

void epicsEventDestroy(epicsEventId id)
{
    semDelete((SEM_ID)id);
}

epicsEventStatus epicsEventWaitWithTimeout(epicsEventId id, double timeout)
{
    int rate = sysClkRateGet();
    int status;
    int ticks;

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
    status = semTake((SEM_ID)id, ticks);
    if (status == OK)
        return epicsEventOK;
    if (errno == S_objLib_OBJ_TIMEOUT ||
        (errno == S_objLib_OBJ_UNAVAILABLE && ticks == 0))
        return epicsEventWaitTimeout;
    return epicsEventError;
}

epicsEventStatus epicsEventTryWait(epicsEventId id)
{
    int status = semTake((SEM_ID)id, NO_WAIT);

    if (status == OK)
        return epicsEventOK;
    if (errno == S_objLib_OBJ_UNAVAILABLE)
        return epicsEventWaitTimeout;
    return epicsEventError;
}

void epicsEventShow(epicsEventId id, unsigned int level)
{
    semShow((SEM_ID)id,level);
}
