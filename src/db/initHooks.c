/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/src/db/initHooks.c*/
/*
 *      Authors:        Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:		06-01-91
 *      major Revision: 07JuL97
 */


#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbDefs.h"
#include "ellLib.h"
#define epicsExportSharedSymbols
#include "initHooks.h"

typedef struct initHookLink {
	ELLNODE		 node;
	initHookFunction func;
} initHookLink;

static int functionListInited = FALSE;
static ELLLIST functionList;

static void initFunctionList(void)
{
    ellInit(&functionList);
    functionListInited = TRUE;
}

/*
 * To be called before iocInit reaches state desired.
 */
int epicsShareAPI initHookRegister(initHookFunction func)
{
	initHookLink *newHook;

	if(!functionListInited) initFunctionList();
	newHook = (initHookLink *)malloc(sizeof(initHookLink));
	if (newHook == NULL)
	{
		printf("Cannot malloc a new initHookLink\n");
		return -1;
	}
	newHook->func = func;
	ellAdd(&functionList,&newHook->node);
	return 0;
}

/*
 * Called by iocInit at various points during initialization.
 * Do not call this function from any other function than iocInit.
 */
void epicsShareAPI initHooks(initHookState state)
{
	initHookLink *hook;

	if(!functionListInited) initFunctionList();
	hook = (initHookLink *)ellFirst(&functionList);
	while(hook != NULL)
	{
		hook->func(state);
		hook = (initHookLink *)ellNext(&hook->node);
	}
}
