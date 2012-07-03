/*************************************************************************\
* Copyright (c) 2012 ITER Organization
*
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Ralph Lange   Date:  28 Jun 2012 */

/* Secure hooks for epicsThread */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsThread.h"

epicsShareFunc void epicsThreadRunStartHooks(epicsThreadId id);
epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadDefaultStartHook;
epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadMainStartHook;

#define checkStatusOnceReturn(status, message, method) \
if((status)) { \
    fprintf(stderr,"%s error %s\n",(message),strerror((status))); \
    fprintf(stderr," %s\n",(method)); \
    return; }

typedef struct epicsThreadHook {
    ELLNODE                   node;
    EPICS_THREAD_HOOK_ROUTINE func;
} epicsThreadHook;

static ELLLIST startHooks = ELLLIST_INIT;

/* Locking could probably be avoided, if elllist implementation was using atomic ops */
static epicsMutexId hookLock;

epicsShareFunc void epicsThreadAddStartHook(EPICS_THREAD_HOOK_ROUTINE hook)
{
    epicsThreadHook *pHook;

    pHook = calloc(1, sizeof(epicsThreadHook));
    if (!pHook) checkStatusOnceReturn(errno,"calloc","epicsThreadAddStartHook");
    pHook->func = hook;
    epicsMutexLock(hookLock);
    ellAdd(&startHooks, &pHook->node);
    epicsMutexUnlock(hookLock);
}

epicsShareFunc void epicsThreadHooksInit(epicsThreadId id)
{
    if (!hookLock) {
        hookLock = epicsMutexMustCreate();
        if (epicsThreadDefaultStartHook) epicsThreadAddStartHook(epicsThreadDefaultStartHook);
    }
    if (id && epicsThreadMainStartHook) epicsThreadMainStartHook(id);
}

epicsShareFunc void epicsThreadRunStartHooks(epicsThreadId id)
{
    epicsThreadHook *pHook;

    epicsMutexLock(hookLock);
    pHook = (epicsThreadHook *) ellFirst(&startHooks);
    while (pHook) {
        pHook->func(id);
        pHook = (epicsThreadHook *) ellNext(&pHook->node);
    }
    epicsMutexUnlock(hookLock);
}
