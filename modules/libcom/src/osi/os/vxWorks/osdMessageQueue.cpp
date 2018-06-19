/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

#define epicsExportSharedSymbols
#include <limits.h>
#include "epicsMessageQueue.h"

extern "C" int sysClkRateGet(void);

epicsShareFunc int epicsShareAPI epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize,
    double timeout)
{
    int ticks;

    if (timeout<=0.0) {
        ticks = 0;
    } else {
        ticks = (int)(timeout*sysClkRateGet());
        if(ticks<=0) ticks = 1;
    }
    return msgQSend((MSG_Q_ID)id, (char *)message, messageSize, ticks, MSG_PRI_NORMAL);
}

epicsShareFunc int epicsShareAPI epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout)
{
    int ticks;

    if (timeout<=0.0) {
        ticks = 0;
    } else {
        ticks = (int)(timeout*sysClkRateGet());
        if(ticks<=0) ticks = 1;
    }
    return msgQReceive((MSG_Q_ID)id, (char *)message, size, ticks);
}
