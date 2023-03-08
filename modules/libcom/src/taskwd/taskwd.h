/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file taskwd.h
 * \brief General purpose task watchdog.
 * \author  Marty Kraimer
 * \date 07-18-91
 * 
 * EPICS provides a task that acts as a watchdog for other tasks. Any task can request
 * to be watched, and most of the IOC tasks do this. A status monitoring subsystem in
 * the IOC can register to be notiﬁed about any changes that occur. The watchdog
 * task runs periodically and checks each task in its task list. If any task is
 * suspended, an error message is displayed and any notiﬁcations made.
 */

#ifndef INC_taskwd_H
#define INC_taskwd_H

#include "epicsThread.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \brief Initialization, optional */
LIBCOM_API void taskwdInit(void);


/** \brief For tasks to be monitored */
typedef void (*TASKWDFUNC)(void *usr);

/** \brief Request by a task to be monitored.
 * 
 * This adds the task with the speciﬁed tid to the list of tasks to be watched, and makes
 * any requested notiﬁcations that a new task has been registered. If tid is given as zero,
 * the epicsThreadId of the calling thread is used instead. If callback is not NULL and
 * the task later becomes suspended, the callback routine will be called with the single argument usr. */
LIBCOM_API void taskwdInsert(epicsThreadId tid,
    TASKWDFUNC callback, void *usr);
/** \brief Remove task from list.
 * 
 * This routine must be called before the monitored task exits. It makes any requested
 * notiﬁcations and removes the task from the list of tasks being watched. If tid is given as
 * zero, the epicsThreadId of the calling thread is used instead. */
LIBCOM_API void taskwdRemove(epicsThreadId tid);

/** \brief Monitoring API */
typedef struct {
    void (*insert)(void *usr, epicsThreadId tid);
    void (*notify)(void *usr, epicsThreadId tid, int suspended);
    void (*remove)(void *usr, epicsThreadId tid);
} taskwdMonitor;

/** \brief Request to be notiﬁed of changes
 * 
 * This call provides a set of callbacks for the task watchdog to call when a task
 * is registered or removed or when any task gets suspended. The usr pointer given at
 * registration is passed to the callback routine along with the tid of the thread the
 * notiﬁcation is about. In many cases the insert and remove callbacks will be called
 * from the context of the thread itself, although this is not guaranteed (the registration
 * could be made by a parent thread for instance). The notify callback also indicates
 * whether the task went into or out of suspension; it is called in both cases, unlike
 * the callbacks registered with taskwdInsert and taskwdAnyInsert. */
LIBCOM_API void taskwdMonitorAdd(const taskwdMonitor *funcs, void *usr);
/** \brief Rescind notiﬁcation request
 * 
 * This call removes a previously registered notiﬁcation. Both funcs and usr must match
 * the values given to taskwdMonitorAdd when originally registered. */
LIBCOM_API void taskwdMonitorDel(const taskwdMonitor *funcs, void *usr);

/** \brief Old monitoring API, deprecated */
typedef void (*TASKWDANYFUNC)(void *usr, epicsThreadId tid);

/** \brief for backwards compatibility purposes, but are now deprecated.
 * 
 * The callback routine will be called whenever any of the tasks being monitored by the task watchdog
 * become suspended. key must have a unique value because the task watchdog system uses this value
 * to determine which entry to remove when taskwdAnyRemove is called. */
LIBCOM_API void taskwdAnyInsert(void *key,
    TASKWDANYFUNC callback, void *usr);
/** \brief for backwards compatibility purposes, but are now deprecated.
 * 
 * key is the same value that was passed to taskwdAnyInsert. */
LIBCOM_API void taskwdAnyRemove(void *key);

/** \brief Print a report
 * 
 * If level is zero, the number of tasks and monitors registered is displayed. For higher
 * values the registered task names and their current states are also shown in tabular form. */
LIBCOM_API void taskwdShow(int level);

#ifdef __cplusplus
}
#endif

#endif /* INCtaskwdH */
