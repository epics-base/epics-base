/* osiRegister.c */
/* Author:  Marty Kraimer Date: 01MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "osiThread.h"
#include "dbEvent.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "osiRegister.h"

/* threadShowAll */
static ioccrfArg threadShowAllArg0 = { "level",ioccrfArgInt,0};
static ioccrfArg *threadShowAllArgs[1] = {&threadShowAllArg0};
static ioccrfFuncDef threadShowAllFuncDef =
    {"threadShowAll",1,threadShowAllArgs};
static void threadShowAllCallFunc(ioccrfArg **args)
{
    threadShowAll(*(int *)args[0]->value);
}

/* threadSleep */
static ioccrfArg threadSleepArg0 = { "seconds",ioccrfArgDouble,0};
static ioccrfArg *threadSleepArgs[1] = {&threadSleepArg0};
static ioccrfFuncDef threadSleepFuncDef =
    {"threadSleep",1,threadSleepArgs};
static void threadSleepCallFunc(ioccrfArg **args)
{
    threadSleep(*(double *)args[0]->value);
}

void epicsShareAPI osiRegister(void)
{
    ioccrfRegister(&threadShowAllFuncDef,threadShowAllCallFunc);
    ioccrfRegister(&threadSleepFuncDef,threadSleepCallFunc);
}
