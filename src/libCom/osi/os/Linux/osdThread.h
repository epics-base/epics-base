/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INC_osdThread_H
#define INC_osdThread_H

#include <pthread.h>
#include <sys/types.h>

#include "shareLib.h"
#include "ellLib.h"
#include "epicsEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct epicsThreadOSD {
    ELLNODE            node;
    pthread_t          tid;
    pid_t              lwpId;
    pthread_attr_t     attr;
    struct sched_param schedParam;
    int                schedPolicy;
    EPICSTHREADFUNC    createFunc;
    void              *createArg;
    epicsEventId       suspendEvent;
    int                isSuspended;
    int                isEpicsThread;
    int                isRealTimeScheduled;
    int                isOnThreadList;
    unsigned int       osiPriority;
    char               name[1];     /* actually larger */
} epicsThreadOSD;

epicsShareFunc pthread_t epicsThreadGetPosixThreadId(epicsThreadId id);
epicsShareFunc int epicsThreadGetPosixPriority(epicsThreadId id);

#ifdef __cplusplus
}
#endif

#endif /* INC_osdThread_H */
