/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "iocsh.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "envDefs.h"
#include "osiUnistd.h"
#include "logClient.h"
#include "errlog.h"
#include "libComRegister.h"


/* chdir */
static const iocshArg chdirArg0 = { "directory name",iocshArgString};
static const iocshArg * const chdirArgs[1] = {&chdirArg0};
static const iocshFuncDef chdirFuncDef = {"cd",1,chdirArgs};
static void chdirCallFunc(const iocshArgBuf *args)
{
    int status;
    status = chdir(args[0].sval);
    if (status) {
        printf ("Invalid directory path ignored\n");
    }
}

/* print current working directory */
static const iocshFuncDef pwdFuncDef = { "pwd", 0, 0 };
static void pwdCallFunc (const iocshArgBuf *args)
{
    char buf[256];
    char *pwd = getcwd ( buf, sizeof(buf) - 1 );
    if ( pwd ) {
        printf ( "%s\n", pwd );
    }
}

/* epicsEnvSet */
static const iocshArg epicsEnvSetArg0 = { "name",iocshArgString};
static const iocshArg epicsEnvSetArg1 = { "value",iocshArgString};
static const iocshArg * const epicsEnvSetArgs[2] = {&epicsEnvSetArg0,&epicsEnvSetArg1};
static const iocshFuncDef epicsEnvSetFuncDef = {"epicsEnvSet",2,epicsEnvSetArgs};
static void epicsEnvSetCallFunc(const iocshArgBuf *args)
{
    char *name = args[0].sval;
    char *value = args[1].sval;

    if (name == NULL) {
        printf ("Missing environment variable name argument.\n");
        return;
    }
    if (value == NULL) {
        printf ("Missing environment variable value argument.\n");
        return;
    }
    epicsEnvSet (name, value);
}

/* epicsParamShow */
static const iocshFuncDef epicsParamShowFuncDef = {"epicsParamShow",0,NULL};
static void epicsParamShowCallFunc(const iocshArgBuf *args)
{
    epicsPrtEnvParams ();
}

/* epicsPrtEnvParams */
static const iocshFuncDef epicsPrtEnvParamsFuncDef = {"epicsPrtEnvParams",0,0};
static void epicsPrtEnvParamsCallFunc(const iocshArgBuf *args)
{
    epicsPrtEnvParams ();
}

/* epicsEnvShow */
static const iocshArg epicsEnvShowArg0 = { "[name]",iocshArgString};
static const iocshArg * const epicsEnvShowArgs[1] = {&epicsEnvShowArg0};
static const iocshFuncDef epicsEnvShowFuncDef = {"epicsEnvShow",1,epicsEnvShowArgs};
static void epicsEnvShowCallFunc(const iocshArgBuf *args)
{
    epicsEnvShow (args[0].sval);
}

/* iocLogInit */
static const iocshFuncDef iocLogInitFuncDef = {"iocLogInit",0};
static void iocLogInitCallFunc(const iocshArgBuf *args)
{
    iocLogInit ();
}

/* iocLogDisable */
static const iocshArg iocLogDisableArg0 = {"(0,1)=>(false,true)",iocshArgInt};
static const iocshArg * const iocLogDisableArgs[1] = {&iocLogDisableArg0};
static const iocshFuncDef iocLogDisableFuncDef = {"setIocLogDisable",1,iocLogDisableArgs};
static void iocLogDisableCallFunc(const iocshArgBuf *args)
{
    iocLogDisable = args[0].ival;
}

/* iocLogShow */
static const iocshArg iocLogShowArg0 = {"level",iocshArgInt};
static const iocshArg * const iocLogShowArgs[1] = {&iocLogShowArg0};
static const iocshFuncDef iocLogShowFuncDef = {"iocLogShow",1,iocLogShowArgs};
static void iocLogShowCallFunc(const iocshArgBuf *args)
{
    iocLogShow (args[0].ival);
}

/* eltc */
static const iocshArg eltcArg0 = {"(0,1)=>(false,true)",iocshArgInt};
static const iocshArg * const eltcArgs[1] = {&eltcArg0};
static const iocshFuncDef eltcFuncDef = {"eltc",1,eltcArgs};
static void eltcCallFunc(const iocshArgBuf *args)
{
    eltc(args[0].ival);
}

/* errlogInit */
static const iocshArg errlogInitArg0 = { "bufsize",iocshArgInt};
static const iocshArg * const errlogInitArgs[1] = {&errlogInitArg0};
static const iocshFuncDef errlogInitFuncDef =
    {"errlogInit",1,errlogInitArgs};
static void errlogInitCallFunc(const iocshArgBuf *args)
{
    errlogInit(args[0].ival);
}

/* errlogInit2 */
static const iocshArg errlogInit2Arg0 = { "bufSize",iocshArgInt};
static const iocshArg errlogInit2Arg1 = { "maxMsgSize",iocshArgInt};
static const iocshArg * const errlogInit2Args[] = 
    {&errlogInit2Arg0, &errlogInit2Arg1};
static const iocshFuncDef errlogInit2FuncDef =
    {"errlogInit2", 2, errlogInit2Args};
static void errlogInit2CallFunc(const iocshArgBuf *args)
{
    errlogInit2(args[0].ival, args[1].ival);
}

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

void epicsShareAPI libComRegister(void)
{
    iocshRegister(&chdirFuncDef, chdirCallFunc);
    iocshRegister(&pwdFuncDef, pwdCallFunc);

    iocshRegister(&epicsEnvSetFuncDef, epicsEnvSetCallFunc);
    iocshRegister(&epicsParamShowFuncDef, epicsParamShowCallFunc);
    iocshRegister(&epicsPrtEnvParamsFuncDef, epicsPrtEnvParamsCallFunc);
    iocshRegister(&epicsEnvShowFuncDef, epicsEnvShowCallFunc);

    iocshRegister(&iocLogInitFuncDef, iocLogInitCallFunc);
    iocshRegister(&iocLogDisableFuncDef, iocLogDisableCallFunc);
    iocshRegister(&iocLogShowFuncDef, iocLogShowCallFunc);
    iocshRegister(&eltcFuncDef, eltcCallFunc);
    iocshRegister(&errlogInitFuncDef,errlogInitCallFunc);
    iocshRegister(&errlogInit2FuncDef,errlogInit2CallFunc);

    iocshRegister(&epicsThreadShowAllFuncDef,epicsThreadShowAllCallFunc);
    iocshRegister(&threadFuncDef, threadCallFunc);
    iocshRegister(&epicsMutexShowAllFuncDef,epicsMutexShowAllCallFunc);
    iocshRegister(&epicsThreadSleepFuncDef,epicsThreadSleepCallFunc);
    iocshRegister(&epicsThreadResumeFuncDef,epicsThreadResumeCallFunc);
}
