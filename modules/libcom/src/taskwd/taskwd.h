/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* General purpose task watchdog */
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

#ifndef INC_taskwd_H
#define INC_taskwd_H

#include "epicsThread.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Initialization, optional */
epicsShareFunc void taskwdInit(void);


/* For tasks to be monitored */
typedef void (*TASKWDFUNC)(void *usr);

epicsShareFunc void taskwdInsert(epicsThreadId tid,
    TASKWDFUNC callback, void *usr);
epicsShareFunc void taskwdRemove(epicsThreadId tid);


/* Monitoring API */
typedef struct {
    void (*insert)(void *usr, epicsThreadId tid);
    void (*notify)(void *usr, epicsThreadId tid, int suspended);
    void (*remove)(void *usr, epicsThreadId tid);
} taskwdMonitor;

epicsShareFunc void taskwdMonitorAdd(const taskwdMonitor *funcs, void *usr);
epicsShareFunc void taskwdMonitorDel(const taskwdMonitor *funcs, void *usr);


/* Old monitoring API, deprecated */
typedef void (*TASKWDANYFUNC)(void *usr, epicsThreadId tid);

epicsShareFunc void taskwdAnyInsert(void *key,
    TASKWDANYFUNC callback, void *usr);
epicsShareFunc void taskwdAnyRemove(void *key);


/* Report function */
epicsShareFunc void taskwdShow(int level);


#ifdef __cplusplus
}
#endif

#endif /* INC_taskwd_H */
