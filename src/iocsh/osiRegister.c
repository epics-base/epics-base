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
static const ioccrfArg threadShowAllArg0 = { "level",ioccrfArgInt};
static const ioccrfArg *threadShowAllArgs[1] = {&threadShowAllArg0};
static const ioccrfFuncDef threadShowAllFuncDef =
    {"threadShowAll",1,threadShowAllArgs};
static void threadShowAllCallFunc(const ioccrfArgBuf *args)
{
    threadShowAll(args[0].ival);
}

/* threadSleep */
static const ioccrfArg threadSleepArg0 = { "seconds",ioccrfArgDouble};
static const ioccrfArg *threadSleepArgs[1] = {&threadSleepArg0};
static const ioccrfFuncDef threadSleepFuncDef =
    {"threadSleep",1,threadSleepArgs};
static void threadSleepCallFunc(const ioccrfArgBuf *args)
{
    threadSleep(args[0].dval);
}

void epicsShareAPI osiRegister(void)
{
    ioccrfRegister(&threadShowAllFuncDef,threadShowAllCallFunc);
    ioccrfRegister(&threadSleepFuncDef,threadSleepCallFunc);
}
