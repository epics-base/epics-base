/* share/src/db/initHooks.c*/
/*
 *      Authors:        Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:		06-01-91
 *      major Revision: 07JuL97
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  09-05-92	rcz	initial version
 * .02  09-10-92	rcz	changed return from void to long
 * .03  09-10-92	rcz	changed completely
 * .04  09-10-92	rcz	bug - moved call to setMasterTimeToSelf later
 * .04  07-15-97        mrk     Benjamin Franksen allow multiple functions
 *
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stddef.h>
#include	<stdio.h>
#include	<ellLib.h>
#include	<initHooks.h>

typedef struct initHookLink {
	ELLNODE		 node;
	initHookFunction func;
} initHookLink;

static functionListInited = FALSE;
static ELLLIST functionList;

static void initFunctionList(void)
{
    ellInit(&functionList);
    functionListInited = TRUE;
}

/*
 * To be called before iocInit reaches state desired.
 */
int initHookRegister(initHookFunction func)
{
	initHookLink *newHook;

	if(!functionListInited) initFunctionList();
	newHook = (initHookLink *)malloc(sizeof(initHookLink));
	if (newHook == NULL)
	{
		printf("Cannot malloc a new initHookLink\n");
		return ERROR;
	}
	newHook->func = func;
	ellAdd(&functionList,&newHook->node);
	return OK;
}

/*
 * Called by iocInit at various points during initialization.
 * Do not call this function from any other function than iocInit.
 */
void initHooks(initHookState state)
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
