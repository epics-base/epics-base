/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef osdThreadh
#define osdThreadh

#include <pthread.h>

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
    EPICSTHREADFUNC    createFunc;
    void              *createArg;
    epicsEventId       suspendEvent;
    int                isSuspended;
    int                isEpicsThread;
    int                isFifoScheduled;
    int                isOnThreadList;
    unsigned int       osiPriority;
    char               name[1];     /* actually larger */
} epicsThreadOSD;

/* Hooks being called when a thread starts or exits */
typedef void (*EPICS_THREAD_HOOK_ROUTINE)(epicsThreadOSD *pthreadInfo);
epicsShareFunc void epicsThreadHooksInit();
epicsShareFunc void epicsThreadAddStartHook(EPICS_THREAD_HOOK_ROUTINE hook);
epicsShareFunc void epicsThreadAddExitHook(EPICS_THREAD_HOOK_ROUTINE hook);
epicsShareFunc void epicsThreadRunStartHooks(epicsThreadOSD *pthreadInfo);
epicsShareFunc void epicsThreadRunExitHooks(epicsThreadOSD *pthreadInfo);
epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadDefaultStartHook;
epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadDefaultExitHook;

epicsShareFunc pthread_t epicsShareAPI epicsThreadGetPosixThreadId(epicsThreadId id);

epicsShareFunc void epicsShowThreadInfo(epicsThreadOSD *pthreadInfo, unsigned int level);

#ifdef __cplusplus
}
#endif

#endif /* osdThreadh */
