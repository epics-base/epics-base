/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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

epicsEventId epicsEventMustCreate(epicsEventInitialState initialState)
{
    epicsEventId id = epicsEventCreate(initialState);
    assert(id);
    return id;
}

void epicsEventDestroy(epicsEventId id)
{
    semDelete((SEM_ID)id);
}

epicsEventWaitStatus epicsEventWaitWithTimeout(epicsEventId id, double timeOut)
{
    int rate = sysClkRateGet();
    int status;
    int ticks;

    if (timeOut <= 0.0) {
        ticks = 0;
    } else if (timeOut >= (double) INT_MAX / rate) {
        ticks = WAIT_FOREVER;
    } else {
        ticks = timeOut * rate;
        if (ticks <= 0)
            ticks = 1;
    }
    status = semTake((SEM_ID)id, ticks);
    if (status == OK)
        return epicsEventWaitOK;
    if (errno == S_objLib_OBJ_TIMEOUT ||
        (errno == S_objLib_OBJ_UNAVAILABLE && ticks == 0))
        return epicsEventWaitTimeout;
    return epicsEventWaitError;
}

epicsEventWaitStatus epicsEventTryWait(epicsEventId id)
{
    int status = semTake((SEM_ID)id, NO_WAIT);

    if (status == OK)
        return epicsEventWaitOK;
    if (errno == S_objLib_OBJ_UNAVAILABLE)
        return epicsEventWaitTimeout;
    return epicsEventWaitError;
}

void epicsEventShow(epicsEventId id, unsigned int level)
{
    semShow((SEM_ID)id,level);
}
