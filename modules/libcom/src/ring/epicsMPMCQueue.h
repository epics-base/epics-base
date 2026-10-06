/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file epicsMPMCQueue.h
 * \brief Bounded lock-free queue of pointers for multiple producers
 *        and multiple consumers
 *
 * Push and pop take no locks, only epicsAtomic operations. They may be
 * called from interrupt context where those are interrupt safe
 * (vxWorks, RTEMS).
 *
 * A thread preempted in the middle of an operation delays others:
 * after an interrupted push, the entries pushed after it can't be popped,
 * and after an interrupted pop, push may report the queue full early.
 */

#ifndef INC_epicsMPMCQueue_H
#define INC_epicsMPMCQueue_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Identifies a queue */
typedef struct epicsMPMCQueue *epicsMPMCQueueId;

/** \brief Create a queue for at least \p size entries
 * (rounded up to a power of two, minimum 2)
 * \return The queue, or NULL on failure */
LIBCOM_API epicsMPMCQueueId epicsStdCall epicsMPMCQueueCreate(int size);
/** \brief Delete a queue; no other thread may use it any more */
LIBCOM_API void epicsStdCall epicsMPMCQueueDelete(epicsMPMCQueueId id);
/** \brief Add \p p at the end
 * \return 1 on success, 0 if the queue is full or \p p is NULL */
LIBCOM_API int epicsStdCall epicsMPMCQueuePush(epicsMPMCQueueId id, void *p);
/** \brief Remove the first entry
 * \return The entry, or NULL if the queue is empty */
LIBCOM_API void* epicsStdCall epicsMPMCQueuePop(epicsMPMCQueueId id);
/** \brief True if no entry is ready to be popped */
LIBCOM_API int epicsStdCall epicsMPMCQueueIsEmpty(epicsMPMCQueueId id);
/** \brief Number of entries the queue can hold */
LIBCOM_API int epicsStdCall epicsMPMCQueueGetSize(epicsMPMCQueueId id);
/** \brief Approximate number of entries in the queue */
LIBCOM_API int epicsStdCall epicsMPMCQueueGetUsed(epicsMPMCQueueId id);
/** \brief Highest number of entries since creation or the last reset */
LIBCOM_API int epicsStdCall epicsMPMCQueueGetHighWaterMark(epicsMPMCQueueId id);
/** \brief Set the high water mark to the current number of entries */
LIBCOM_API void epicsStdCall epicsMPMCQueueResetHighWaterMark(epicsMPMCQueueId id);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsMPMCQueue_H */
