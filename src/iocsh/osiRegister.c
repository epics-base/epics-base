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

#include "epicsThread.h"
#include "epicsMutex.h"
#include "dbEvent.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "osiRegister.h"

/* epicsThreadShowAll */
static const ioccrfArg epicsThreadShowAllArg0 = { "level",ioccrfArgInt};
static const ioccrfArg * const epicsThreadShowAllArgs[1] = {&epicsThreadShowAllArg0};
static const ioccrfFuncDef epicsThreadShowAllFuncDef =
    {"epicsThreadShowAll",1,epicsThreadShowAllArgs};
static void epicsThreadShowAllCallFunc(const ioccrfArgBuf *args)
{
    epicsThreadShowAll(args[0].ival);
}

/* epicsMutexShowAll */
static const ioccrfArg epicsMutexShowAllArg0 = { "onlyLocked",ioccrfArgInt};
static const ioccrfArg epicsMutexShowAllArg1 = { "level",ioccrfArgInt};
static const ioccrfArg * const epicsMutexShowAllArgs[2] =
    {&epicsMutexShowAllArg0,&epicsMutexShowAllArg1};
static const ioccrfFuncDef epicsMutexShowAllFuncDef =
    {"epicsMutexShowAll",1,epicsMutexShowAllArgs};
static void epicsMutexShowAllCallFunc(const ioccrfArgBuf *args)
{
    epicsMutexShowAll(args[0].ival,args[1].ival);
}

/* epicsThreadSleep */
static const ioccrfArg epicsThreadSleepArg0 = { "seconds",ioccrfArgDouble};
static const ioccrfArg * const epicsThreadSleepArgs[1] = {&epicsThreadSleepArg0};
static const ioccrfFuncDef epicsThreadSleepFuncDef =
    {"epicsThreadSleep",1,epicsThreadSleepArgs};
static void epicsThreadSleepCallFunc(const ioccrfArgBuf *args)
{
    epicsThreadSleep(args[0].dval);
}

void epicsShareAPI osiRegister(void)
{
    ioccrfRegister(&epicsThreadShowAllFuncDef,epicsThreadShowAllCallFunc);
    ioccrfRegister(&epicsMutexShowAllFuncDef,epicsMutexShowAllCallFunc);
    ioccrfRegister(&epicsThreadSleepFuncDef,epicsThreadSleepCallFunc);
}
