/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* src/db/initHooks.c */
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
#include "epicsThread.h"
#define epicsExportSharedSymbols
#include "initHooks.h"

typedef struct initHookLink {
    ELLNODE          node;
    initHookFunction func;
} initHookLink;

static ELLLIST functionList;

/*
 * Lazy initialization functions
 */
static void initHookOnce(void *arg)
{
    ellInit(&functionList);
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

    initHookInit();
    newHook = (initHookLink *)malloc(sizeof(initHookLink));
    if (newHook == NULL) {
        printf("Cannot malloc a new initHookLink\n");
        return -1;
    }
    newHook->func = func;
    ellAdd(&functionList, &newHook->node);
    return 0;
}

/*
 * Called by iocInit at various points during initialization.
 * Do not call this function from any other function than iocInit.
 */
void initHooks(initHookState state)
{
    initHookLink *hook;

    initHookInit();
    hook = (initHookLink *)ellFirst(&functionList);
    while (hook != NULL) {
        hook->func(state);
        hook = (initHookLink *)ellNext(&hook->node);
    }
}

/*
 * Call any time you want to print out a state name.
 */
const char *initHookName(initHookState state)
{
    const char *stateName[] = {
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
        "initHookAfterInterruptAccept",
        "initHookAtEnd"
    };
    if (state < initHookAtBeginning || state > initHookAtEnd) {
        return "Not an initHookState";
    }
    return stateName[state];
}
