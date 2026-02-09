/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* includes for general purpose callback tasks          */
/*
 *      Original Author:        Marty Kraimer
 *      Date:                   07-18-91
*/

#ifndef INCcallbackh
#define INCcallbackh 1

#include "dbCoreAPI.h"

/** @file callback.h
 *  @brief Process database deferred execution utility
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WINDOWS also has a "CALLBACK" type def
 */
#if defined(_WIN32) && !defined(EPICS_NO_CALLBACK)
#       ifdef CALLBACK
#               undef CALLBACK
#       endif /*CALLBACK*/
#endif /*_WIN32*/

#define NUM_CALLBACK_PRIORITIES 3
#define priorityLow     0
#define priorityMedium  1
#define priorityHigh    2

/** Handle for delayed work.
 *
 * @pre Must be zero initialized prior to first use.
 *
 * @since 3.15.6 epicsCallback typedef added.  CALLBACK typedef deprecated.
 */
typedef struct callbackPvt {
        /** Callback function */
        void (*callback)(struct callbackPvt*);
        /** One of priorityLow, priorityMedium, or priorityHigh */
        int             priority;
        /** for use by callback API user*/
        void            *user;
        /** Must be zero initialized.  Used by callback internals. */
        void            *timer;
}epicsCallback;

#if !defined(EPICS_NO_CALLBACK)
/** Deprecated alias for epicsCallback
 *
 * Name conflicts with definition from windows.h.
 * Portable applications should prefer epicsCallback
 * and define the EPICS_NO_CALLBACK pre-processor macro to hide this typedef.
 *
 * @since 3.15.6 Deprecated in favor of epicsCallback typedef
 */
typedef epicsCallback CALLBACK;
#endif

typedef void    (*CALLBACKFUNC)(struct callbackPvt*);
/** See callbackQueueStatus() */
typedef struct callbackQueueStats {
    /** Maxiumum depth of queues */
    int size;
    /** Current number of elements on each queue */
    int numUsed[NUM_CALLBACK_PRIORITIES];
    /** Maximum numUsed seen so far (from init or reset)  */
    int maxUsed[NUM_CALLBACK_PRIORITIES];
    /** Number of overflow events */
    int numOverflow[NUM_CALLBACK_PRIORITIES];
} callbackQueueStats;

/** Assigns callbackPvt::callback */
#define callbackSetCallback(PFUN, PCALLBACK) \
    ( (PCALLBACK)->callback = (PFUN) )
/** Assigns callbackPvt::priority */
#define callbackSetPriority(PRIORITY, PCALLBACK) \
    ( (PCALLBACK)->priority = (PRIORITY) )
/** Assigns callbackPvt::priority */
#define callbackGetPriority(PRIORITY, PCALLBACK) \
    ( (PRIORITY) = (PCALLBACK)->priority )
/** Assigns callbackPvt::user */
#define callbackSetUser(USER, PCALLBACK) \
    ( (PCALLBACK)->user = (USER) )
/** Read and return callbackPvt::user */
#define callbackGetUser(USER, PCALLBACK) \
    ( (USER) = (PCALLBACK)->user )

DBCORE_API void callbackInit(void);
DBCORE_API void callbackStop(void);
DBCORE_API void callbackCleanup(void);
/** Queue immediate callback request.
 *
 * Each epicsCallback may be queued multiple times.
 * epicsCallback object must not be modified while queued,
 * and must remain valid while queued or executing.
 *
 * @param pCallback Caller expected to initialize or zero all members before first call.
 * @return Zero on success.
 *         Errors if callback members not initialized correctly, or if queue is full.
 */
DBCORE_API int callbackRequest(epicsCallback *pCallback);
/** Setup callback to process a record
 * @pre Callback must be zero initialized.
 *
 * @param pcallback Callback to initialize.
 * @param Priority priorityLow, priorityMedium, or priorityHigh
 * @param pRec A record pointer (dbCommon or specific recordType)
 */
DBCORE_API void callbackSetProcess(
    epicsCallback *pcallback, int Priority, void *pRec);
/** (Re)Initialize callback object and queue
 *
 *  Shorthand for callbackSetProcess() followed by callbackRequest()
 *
 * @pre Callback object must be zero initialized before first call.
 */
DBCORE_API int callbackRequestProcessCallback(
    epicsCallback *pCallback,int Priority, void *pRec);
/** Queue delayed callback request
 *
 * Each epicsCallback has a single timer.
 * Repeated calls before expiration will cancel and reschedule timer.
 * epicsCallback object must not be modified while queued,
 * and must remain valid while queued or executing.
 *
 * epicsCallback::timer must be zeroed before the first call,
 * and left unmodified for subsequent calls.
 * Each epicsCallback is allocated a timer on first call.
 * There is no way to free this allocation.
 * Reuse of epicsCallback is strongly recommended.
 *
 * @param pCallback Callback object.
 *        Caller expected to initialize or zero all members prior to first call.
 * @param seconds Relative to call time.  Expected to be >= 0.
 * @return Zero on success.
 *         Errors if callback members not initialized correctly, or if queue is full.
 */
DBCORE_API void callbackRequestDelayed(
    epicsCallback *pCallback,double seconds);
/** Cancel delayed callback.
 *
 * Usage not recommended.  Caller can not distinguish between successful
 * cancellation, or expiration.  In the later case the callback may still be
 * queued or executing.
 *
 * @param pcallback Callback object previously passed to callbackRequestDelayed()
 *
 * @post Timer is cancelled.  However, callback may be queued or executing.
 */
DBCORE_API void callbackCancelDelayed(epicsCallback *pcallback);
/** (Re)Initialize callback object and queue
 *
 *  Shorthand for callbackSetProcess() followed by callbackRequestDelayed()
 *
 * @pre Callback object must be zero initialized before first call.
 */
DBCORE_API void callbackRequestProcessCallbackDelayed(
    epicsCallback *pCallback, int Priority, void *pRec, double seconds);
/** Set callback queue depth
 *
 * @param size A positive integer
 * @return -1 if too late to change depth
 *
 * @pre Must be called before iocInit()
 */
DBCORE_API int callbackSetQueueSize(int size);
/** Query configuration and statistics from callback system
 * @param reset If non-zero, reset maxUsed after reading.
 * @param result NULL, or location for results
 * @return -2 if result is NULL.  reset happens anyway.
 *
 * @since 7.0.2.  Also present in 3.16.2
 */
DBCORE_API int callbackQueueStatus(const int reset, callbackQueueStats *result);
DBCORE_API void callbackQueueShow(const int reset);
/** Setup multiple worker threads for specified priority
 *
 * By default, only one thread is run for each priority (3 in total).
 *
 * Calling with count==0 will take the count from the callbackParallelThreadsDefault
 * global variable (initialized to the number of CPU cores initially available to
 * the main thread).
 * Calling with count>0 sets the number of worker threads directly.
 * Calling with count<0 computes the count based on the number of CPU cores
 * currently available to the calling thread,
 * eg. Passing -2 on an 8 core system will start 6 worker threads.
 * The iocsh wrapper also accepts positive or negative percentages for count,
 * which allows to refer to a fraction of the currently available CPU cores,
 * always rounding the number of created threads down.
 * In any case, at least one worker thread will always run.
 *
 * An empty prio name or the special value "*" will modify all priorities.
 * Otherwise, only the named priority is modified.
 * The prio names are case insensitive.
 *
 * @param count If zero, reset to default (callbackParallelThreadsDefault global/iocsh variable).
 *              If positive, exact number of worker threads to create.
 *              If negative, reserve this many CPUs to _not_ run parallel callback threads.
 * @param prio Priority name.  eg. "*", "LOW", "MEDIUM" or "HIGH".
 * @return zero on success, non-zero if called after iocInit() or with invalid arguments.
 *
 * @pre Must be called before iocInit()
 *
 * @since 3.15.0.2
 */
DBCORE_API int callbackParallelThreads(int count, const char *prio);

#ifdef __cplusplus
}
#endif

#endif /*INCcallbackh*/
