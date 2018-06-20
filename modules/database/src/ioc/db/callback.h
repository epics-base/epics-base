/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* includes for general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

#ifndef INCcallbackh
#define INCcallbackh 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WINDOWS also has a "CALLBACK" type def
 */
#ifdef _WIN32
#	ifdef CALLBACK
#		undef CALLBACK
#	endif /*CALLBACK*/
#endif /*_WIN32*/

#define NUM_CALLBACK_PRIORITIES 3
#define priorityLow     0
#define priorityMedium  1
#define priorityHigh    2

typedef struct callbackPvt {
	void (*callback)(struct callbackPvt*);
	int		priority;
	void		*user; /*for use by callback user*/
        void            *timer; /*for use by callback itself*/
}CALLBACK;

typedef void    (*CALLBACKFUNC)(struct callbackPvt*);

#define callbackSetCallback(PFUN, PCALLBACK) \
    ( (PCALLBACK)->callback = (PFUN) )
#define callbackSetPriority(PRIORITY, PCALLBACK) \
    ( (PCALLBACK)->priority = (PRIORITY) )
#define callbackGetPriority(PRIORITY, PCALLBACK) \
    ( (PRIORITY) = (PCALLBACK)->priority )
#define callbackSetUser(USER, PCALLBACK) \
    ( (PCALLBACK)->user = (void *) (USER) )
#define callbackGetUser(USER, PCALLBACK) \
    ( (USER) = (PCALLBACK)->user )

epicsShareFunc void callbackInit(void);
epicsShareFunc void callbackStop(void);
epicsShareFunc void callbackCleanup(void);
epicsShareFunc int callbackRequest(CALLBACK *pCallback);
epicsShareFunc void callbackSetProcess(
    CALLBACK *pcallback, int Priority, void *pRec);
epicsShareFunc int callbackRequestProcessCallback(
    CALLBACK *pCallback,int Priority, void *pRec);
epicsShareFunc void callbackRequestDelayed(
    CALLBACK *pCallback,double seconds);
epicsShareFunc void callbackCancelDelayed(CALLBACK *pcallback);
epicsShareFunc void callbackRequestProcessCallbackDelayed(
    CALLBACK *pCallback, int Priority, void *pRec, double seconds);
epicsShareFunc int callbackSetQueueSize(int size);
epicsShareFunc int callbackParallelThreads(int count, const char *prio);

#ifdef __cplusplus
}
#endif

#endif /*INCcallbackh*/
