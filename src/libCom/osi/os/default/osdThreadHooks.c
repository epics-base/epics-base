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
static ELLLIST exitHooks = ELLLIST_INIT;
static epicsMutexId hookLock;

static void addHook (ELLLIST *list, EPICS_THREAD_HOOK_ROUTINE func)
{
    epicsThreadHook *pHook;

    pHook = calloc(1, sizeof(epicsThreadHook));
    if (!pHook) checkStatusOnceReturn(errno,"calloc","epicsThreadAddStartHook");
    pHook->func = func;
    epicsMutexLock(hookLock);
    ellInsert(list, NULL, &pHook->node);
    epicsMutexUnlock(hookLock);
}

static void runHooks (ELLLIST *list, epicsThreadId id) {
    epicsThreadHook *pHook;

    /* As we're only ever inserting hooks at the head of the list, forward traversing is safe */
    pHook = (epicsThreadHook *) ellFirst(list);
    while (pHook) {
        pHook->func(id);
        pHook = (epicsThreadHook *) ellNext(&pHook->node);
    }
}

epicsShareFunc void epicsShareAPI epicsThreadAddStartHook(EPICS_THREAD_HOOK_ROUTINE hook)
{
    addHook(&startHooks, hook);
}

epicsShareFunc void epicsShareAPI epicsThreadAddExitHook(EPICS_THREAD_HOOK_ROUTINE hook)
{
    addHook(&exitHooks, hook);
}

epicsShareFunc void epicsShareAPI epicsThreadHooksInit(void)
{
    if (!hookLock) {
        hookLock = epicsMutexMustCreate();
        if (epicsThreadDefaultStartHook) epicsThreadAddStartHook(epicsThreadDefaultStartHook);
        if (epicsThreadDefaultExitHook) epicsThreadAddExitHook(epicsThreadDefaultExitHook);
    }
}

epicsShareFunc void epicsShareAPI epicsThreadRunStartHooks(epicsThreadId id)
{
    runHooks(&startHooks, id);
}

epicsShareFunc void epicsShareAPI epicsThreadRunExitHooks(epicsThreadId id)
{
    runHooks(&exitHooks, id);
}
