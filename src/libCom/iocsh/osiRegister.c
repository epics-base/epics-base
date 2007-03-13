/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "iocsh.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#define epicsExportSharedSymbols
#include "osiRegister.h"

/* epicsThreadShowAll */
static const iocshArg epicsThreadShowAllArg0 = { "level",iocshArgInt};
static const iocshArg * const epicsThreadShowAllArgs[1] = {&epicsThreadShowAllArg0};
static const iocshFuncDef epicsThreadShowAllFuncDef =
    {"epicsThreadShowAll",1,epicsThreadShowAllArgs};
static void epicsThreadShowAllCallFunc(const iocshArgBuf *args)
{
    epicsThreadShowAll(args[0].ival);
}

/* thread (thread information) */
static const iocshArg threadArg0 = { "[-level] [thread ...]", iocshArgArgv};
static const iocshArg * const threadArgs[1] = { &threadArg0 };
static const iocshFuncDef threadFuncDef = {"thread",1,threadArgs};
static void threadCallFunc(const iocshArgBuf *args)
{
    int i = 1;
    int first = 1;
    int level = 0;
    const char *cp;
    epicsThreadId tid;
    unsigned long ltmp;
    int argc = args[0].aval.ac;
    char **argv = args[0].aval.av;
    char *endp;

    if ((i < argc) && (*(cp = argv[i]) == '-')) {
        level = atoi (cp + 1);
        i++;
    }
    if (i >= argc) {
        epicsThreadShowAll (level);
        return;
    }
    for ( ; i < argc ; i++) {
        cp = argv[i];
        ltmp = strtoul (cp, &endp, 0);
        if (*endp) {
            tid = epicsThreadGetId (cp);
            if (!tid) {
                printf ("*** argument %d (%s) is not a valid thread name ***\n", i, cp);
                continue;
            }
        }
        else {
            tid = (epicsThreadId)ltmp;
        }
        if (first) {
            epicsThreadShow (0, level);
            first = 0;
        }
        epicsThreadShow (tid, level);
    }
}

/* epicsMutexShowAll */
static const iocshArg epicsMutexShowAllArg0 = { "onlyLocked",iocshArgInt};
static const iocshArg epicsMutexShowAllArg1 = { "level",iocshArgInt};
static const iocshArg * const epicsMutexShowAllArgs[2] =
    {&epicsMutexShowAllArg0,&epicsMutexShowAllArg1};
static const iocshFuncDef epicsMutexShowAllFuncDef =
    {"epicsMutexShowAll",1,epicsMutexShowAllArgs};
static void epicsMutexShowAllCallFunc(const iocshArgBuf *args)
{
    epicsMutexShowAll(args[0].ival,args[1].ival);
}

/* epicsThreadSleep */
static const iocshArg epicsThreadSleepArg0 = { "seconds",iocshArgDouble};
static const iocshArg * const epicsThreadSleepArgs[1] = {&epicsThreadSleepArg0};
static const iocshFuncDef epicsThreadSleepFuncDef =
    {"epicsThreadSleep",1,epicsThreadSleepArgs};
static void epicsThreadSleepCallFunc(const iocshArgBuf *args)
{
    epicsThreadSleep(args[0].dval);
}

/* epicsThreadResume */
static const iocshArg epicsThreadResumeArg0 = { "[thread ...]", iocshArgArgv};
static const iocshArg * const epicsThreadResumeArgs[1] = { &epicsThreadResumeArg0 };
static const iocshFuncDef epicsThreadResumeFuncDef = {"epicsThreadResume",1,epicsThreadResumeArgs};
static void epicsThreadResumeCallFunc(const iocshArgBuf *args)
{
    int i;
    const char *cp;
    epicsThreadId tid;
    unsigned long ltmp;
    char nameBuf[64];
    int argc = args[0].aval.ac;
    char **argv = args[0].aval.av;
    char *endp;

    for (i = 1 ; i < argc ; i++) {
        cp = argv[i];
        ltmp = strtoul(cp, &endp, 0);
        if (*endp) {
            tid = epicsThreadGetId(cp);
            if (!tid) {
                printf("*** argument %d (%s) is not a valid thread name ***\n", i, cp);
                continue;
            }
        }
        else {
            tid =(epicsThreadId)ltmp;
            epicsThreadGetName(tid, nameBuf, sizeof nameBuf);
            if (nameBuf[0] == '\0') {
                printf("*** argument %d (%s) is not a valid thread id ***\n", i, cp);
                continue;
            }

        }
        if (!epicsThreadIsSuspended(tid)) {
            printf("*** Thread %s is not suspended ***\n", cp);
            continue;
        }
        epicsThreadResume(tid);
    }
}

void epicsShareAPI osiRegister(void)
{
    iocshRegister(&epicsThreadShowAllFuncDef,epicsThreadShowAllCallFunc);
    iocshRegister(&threadFuncDef, threadCallFunc);
    iocshRegister(&epicsMutexShowAllFuncDef,epicsMutexShowAllCallFunc);
    iocshRegister(&epicsThreadSleepFuncDef,epicsThreadSleepCallFunc);
    iocshRegister(&epicsThreadResumeFuncDef,epicsThreadResumeCallFunc);
}
