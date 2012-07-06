/*************************************************************************\
* Copyright (c) 2012 ITER Organization
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
*
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Authors:  Ralph Lange & Andrew Johnson */

/* Secure hooks for epicsThread */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsThread.h"

epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadHookDefault;
epicsShareExtern EPICS_THREAD_HOOK_ROUTINE epicsThreadHookMain;

typedef struct epicsThreadHook {
    ELLNODE                   node;
    EPICS_THREAD_HOOK_ROUTINE func;
} epicsThreadHook;

static ELLLIST hookList = ELLLIST_INIT;
static epicsMutexId hookLock;


static void threadHookOnce(void *arg)
{
    hookLock = epicsMutexMustCreate();

    if (epicsThreadHookDefault) {
        static epicsThreadHook defHook = {ELLNODE_INIT, NULL};

        defHook.func = epicsThreadHookDefault;
        ellAdd(&hookList, &defHook.node);
    }
}

static void threadHookInit(void)
{
    static epicsThreadOnceId flag = EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce(&flag, threadHookOnce, NULL);
}

epicsShareFunc int epicsThreadHookAdd(EPICS_THREAD_HOOK_ROUTINE hook)
{
    epicsThreadHook *pHook;

    if (!hook) return 0;
    threadHookInit();

    pHook = calloc(1, sizeof(epicsThreadHook));
    if (!pHook) {
        fprintf(stderr, "epicsThreadHookAdd: calloc failed\n");
        return -1;
    }
    pHook->func = hook;

    if (epicsMutexLock(hookLock) == epicsMutexLockOK) {
        ellAdd(&hookList, &pHook->node);
        epicsMutexUnlock(hookLock);
        return 0;
    }
    fprintf(stderr, "epicsThreadHookAdd: Locking problem\n");
    return -1;
}

epicsShareFunc int epicsThreadHookDelete(EPICS_THREAD_HOOK_ROUTINE hook)
{
    if (!hook) return 0;
    threadHookInit();

    if (epicsMutexLock(hookLock) == epicsMutexLockOK) {
        epicsThreadHook *pHook = (epicsThreadHook *) ellFirst(&hookList);

        while (pHook) {
            if (hook == pHook->func) {
                ellDelete(&hookList, &pHook->node);
                break;
            }
            pHook = (epicsThreadHook *) ellNext(&pHook->node);
        }
        epicsMutexUnlock(hookLock);
        return 0;
    }
    fprintf(stderr, "epicsThreadHookAdd: Locking problem\n");
    return -1;
}

epicsShareFunc void osdThreadHooksRunMain(epicsThreadId id)
{
    if (epicsThreadHookMain)
        epicsThreadHookMain(id);
}

epicsShareFunc void osdThreadHooksRun(epicsThreadId id)
{
    threadHookInit();

    if (epicsMutexLock(hookLock) == epicsMutexLockOK) {
        epicsThreadHook *pHook = (epicsThreadHook *) ellFirst(&hookList);

        while (pHook) {
            pHook->func(id);
            pHook = (epicsThreadHook *) ellNext(&pHook->node);
        }
        epicsMutexUnlock(hookLock);
    }
    else {
        fprintf(stderr, "osdThreadHooksRun: Locking problem\n");
    }
}

epicsShareFunc void epicsThreadHooksShow(void)
{
    threadHookInit();

    if (epicsMutexLock(hookLock) == epicsMutexLockOK) {
        epicsThreadHook *pHook = (epicsThreadHook *) ellFirst(&hookList);

        while (pHook) {
            printf("  %p\n", pHook->func);
            pHook = (epicsThreadHook *) ellNext(&pHook->node);
        }
        epicsMutexUnlock(hookLock);
    }
    else {
        fprintf(stderr, "epicsThreadHooksShow: Locking problem\n");
    }
}
