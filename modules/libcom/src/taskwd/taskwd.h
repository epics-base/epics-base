/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* General purpose task watchdog */
/*
 *      Original Author:        Marty Kraimer
 *      Date:                   07-18-91
*/

#ifndef INC_taskwd_H
#define INC_taskwd_H

#include "epicsThread.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Initialization, optional */
LIBCOM_API void taskwdInit(void);


/* For tasks to be monitored */
typedef void (*TASKWDFUNC)(void *usr);

LIBCOM_API void taskwdInsert(epicsThreadId tid,
    TASKWDFUNC callback, void *usr);
LIBCOM_API void taskwdRemove(epicsThreadId tid);


/* Monitoring API */
typedef struct {
    void (*insert)(void *usr, epicsThreadId tid);
    void (*notify)(void *usr, epicsThreadId tid, int suspended);
    void (*remove)(void *usr, epicsThreadId tid);
} taskwdMonitor;

LIBCOM_API void taskwdMonitorAdd(const taskwdMonitor *funcs, void *usr);
LIBCOM_API void taskwdMonitorDel(const taskwdMonitor *funcs, void *usr);


/* Old monitoring API, deprecated */
typedef void (*TASKWDANYFUNC)(void *usr, epicsThreadId tid);

LIBCOM_API void taskwdAnyInsert(void *key,
    TASKWDANYFUNC callback, void *usr);
LIBCOM_API void taskwdAnyRemove(void *key);


/* Report function */
LIBCOM_API void taskwdShow(int level);


#ifdef __cplusplus
}
#endif

#endif /* INC_taskwd_H */
