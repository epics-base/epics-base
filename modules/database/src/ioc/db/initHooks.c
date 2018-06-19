/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Authors:        Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:           06-01-91
 *      major Revision: 07JuL97
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsThread.h"

#define epicsExportSharedSymbols
#include "initHooks.h"

typedef struct initHookLink {
    ELLNODE          node;
    initHookFunction func;
} initHookLink;

static ELLLIST functionList = ELLLIST_INIT;
static epicsMutexId listLock;

/*
 * Lazy initialization functions
 */
static void initHookOnce(void *arg)
{
    listLock = epicsMutexMustCreate();
}

static void initHookInit(void)
{
    static epicsThreadOnceId onceFlag = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce(&onceFlag, initHookOnce, NULL);
}

/*
 * To be called before iocInit reaches state desired.
 */
int initHookRegister(initHookFunction func)
{
    initHookLink *newHook;

    if (!func) return 0;

    initHookInit();

    newHook = (initHookLink *)malloc(sizeof(initHookLink));
    if (!newHook) {
        printf("Cannot malloc a new initHookLink\n");
        return -1;
    }
    newHook->func = func;

    epicsMutexMustLock(listLock);
    ellAdd(&functionList, &newHook->node);
    epicsMutexUnlock(listLock);
    return 0;
}

/*
 * Called by iocInit at various points during initialization.
 * This function must only be called by iocInit and relatives.
 */
void initHookAnnounce(initHookState state)
{
    initHookLink *hook;

    initHookInit();

    epicsMutexMustLock(listLock);
    hook = (initHookLink *)ellFirst(&functionList);
    epicsMutexUnlock(listLock);

    while (hook != NULL) {
        hook->func(state);

        epicsMutexMustLock(listLock);
        hook = (initHookLink *)ellNext(&hook->node);
        epicsMutexUnlock(listLock);
    }
}

void initHookFree(void)
{
    initHookInit();
    epicsMutexMustLock(listLock);
    ellFree(&functionList);
    epicsMutexUnlock(listLock);
}

/*
 * Call any time you want to print out a state name.
 */
const char *initHookName(int state)
{
    const char *stateName[] = {
        "initHookAtIocBuild",
        "initHookAtBeginning",
        "initHookAfterCallbackInit",
        "initHookAfterCaLinkInit",
        "initHookAfterInitDrvSup",
        "initHookAfterInitRecSup",
        "initHookAfterInitDevSup",
        "initHookAfterInitDatabase",
        "initHookAfterFinishDevSup",
        "initHookAfterScanInit",
        "initHookAfterInitialProcess",
        "initHookAfterCaServerInit",
        "initHookAfterIocBuilt",
        "initHookAtIocRun",
        "initHookAfterDatabaseRunning",
        "initHookAfterCaServerRunning",
        "initHookAfterIocRunning",
        "initHookAtIocPause",
        "initHookAfterCaServerPaused",
        "initHookAfterDatabasePaused",
        "initHookAfterIocPaused",
        "initHookAfterInterruptAccept",
        "initHookAtEnd"
    };
    if (state < 0 || state >= NELEMENTS(stateName)) {
        return "Not an initHookState";
    }
    return stateName[state];
}
