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

/**
 * \file callback.h
 * \brief includes definitions for general purpose callback tasks
 * \author  Marty Kraimer
 * \date 07-18-91
 */

#ifndef INCcallbackh
#define INCcallbackh 1

#include "dbCoreAPI.h"

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

typedef struct callbackPvt {
        void (*callback)(struct callbackPvt*);
        int             priority;
        void            *user; /** \brief for use by callback user*/
        void            *timer; /** \brief for use by callback itself*/
}epicsCallback;

#if !defined(EPICS_NO_CALLBACK)
typedef epicsCallback CALLBACK;
#endif

typedef void    (*CALLBACKFUNC)(struct callbackPvt*);

typedef struct callbackQueueStats {
    int size;
    int numUsed[NUM_CALLBACK_PRIORITIES];
    int maxUsed[NUM_CALLBACK_PRIORITIES];
    int numOverflow[NUM_CALLBACK_PRIORITIES];
} callbackQueueStats;

/** \brief Make calls (in most cases these are actually macros) to initialize the ﬁelds in the CALLBACK */
#define callbackSetCallback(PFUN, PCALLBACK) \
    ( (PCALLBACK)->callback = (PFUN) )
/** \brief Deﬁnes the callback routine to be executed.
 * 
 * The ﬁrst argument is the address of a function that will be given the address of the CALLBACK
 * and returns void. The second argument is the address of the CALLBACK structure. 
 */
#define callbackSetPriority(PRIORITY, PCALLBACK) \
    ( (PCALLBACK)->priority = (PRIORITY) )
/** \brief Deﬁnes the callback routine to be executed. */
#define callbackGetPriority(PRIORITY, PCALLBACK) \
    ( (PRIORITY) = (PCALLBACK)->priority )
/**
 * The ﬁrst argument is the priority, which can have one of the values: priorityLow, priorityMedium,
 * or priorityHigh. These values are deﬁned in callback.h. The second argument is again the address of the CALLBACK structure.
 */
#define callbackSetUser(USER, PCALLBACK) \
    ( (PCALLBACK)->user = (void *) (USER) )
/** \brief This call is used to save a pointer value that can be retrieved again using the macro. */
#define callbackGetUser(USER, PCALLBACK) \
    ( (USER) = (PCALLBACK)->user )

DBCORE_API void callbackInit(void);
DBCORE_API void callbackStop(void);
DBCORE_API void callbackCleanup(void);

/** \brief callback request
 * 
 * Can be called from interrupt level code. The callback routine is passed a single argument, which is
 * the same argument that was passed to callbackRequest, i.e., the address of the CALLBACK structure.
 * Returns zero in case of success, or an error code (see below). 
 */
DBCORE_API int callbackRequest(epicsCallback *pCallback);
/** zbrief callback shortcut
 * 
 * If your callback function exists to process a single record inside calls to dbScanLock/dbScanUnlock,
 * you can use this shortcut which provides the callback routine for you and sets the other
 * two parameters at the same time (the user parameter here is a pointer to the record instance)
 */
DBCORE_API void callbackSetProcess(
    epicsCallback *pcallback, int Priority, void *pRec);
/** \brief callback request
 * 
 * Can be called from interrupt level code. is a shortcut for calling both callbackSetProcess
 * and callbackRequest Returns zero in case of success, or an error code (see below). 
 */
DBCORE_API int callbackRequestProcessCallback(
    epicsCallback *pCallback,int Priority, void *pRec);
/** \brief Delayed version of callbackRequest.
 * 
 * The Delayed version waits for the given time before queueing the callback routine for the relevant thread set to execute.
 */
DBCORE_API void callbackRequestDelayed(
    epicsCallback *pCallback,double seconds);
/** \brief can be used to cancel a delayed callback */
DBCORE_API void callbackCancelDelayed(epicsCallback *pcallback);
/** \brief Delayed version of callbackRequestProcessCallback.
 * 
 * The Delayed version waits for the given time before queueing the callback routine for the relevant thread set to execute.
 */
DBCORE_API void callbackRequestProcessCallbackDelayed(
    epicsCallback *pCallback, int Priority, void *pRec, double seconds);
/** 
 * \brief Set priority limit.
 * 
 * The callback requests put the requests for each callback priority into a separate ring buﬀer.
 * These buﬀers can by default hold up to 2000 requests. This limit can be changed by calling
 * callbackSetQueueSize before iocInit in the startup ﬁle. 
 */
DBCORE_API int callbackSetQueueSize(int size);
DBCORE_API int callbackQueueStatus(const int reset, callbackQueueStats *result);
DBCORE_API void callbackQueueShow(const int reset);
/** \brief Called before iocInit in the startup ﬁle to enable multiple parallel callback tasks, and set the number of tasks to be started for each priority level.
 * \param count the number of tasks to start
 * \param prio speciﬁes the priority level
 */
DBCORE_API int callbackParallelThreads(int count, const char *prio);

#ifdef __cplusplus
}
#endif

#endif /*INCcallbackh*/
