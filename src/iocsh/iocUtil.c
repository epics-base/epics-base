/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocUtil.c */
/* Author:  W. Eric Norum Date: 02MAY2000 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <envDefs.h>

#include "osiUnistd.h"
#include "epicsThread.h"
#include "logClient.h"
#include "errlog.h"
#include "epicsRelease.h"

#define epicsExportSharedSymbols
#include "iocsh.h"
#include "iocUtilRegister.h"

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

/* coreRelease */
static const iocshFuncDef coreReleaseFuncDef = {"coreRelease",0,NULL};
static void coreReleaseCallFunc(const iocshArgBuf *args)
{
    coreRelease ();
}

void epicsShareAPI iocUtilRegister(void)
{
    iocshRegister(&chdirFuncDef,chdirCallFunc);
    iocshRegister(&pwdFuncDef,pwdCallFunc);
    iocshRegister(&threadFuncDef,threadCallFunc);
    iocshRegister(&epicsEnvSetFuncDef,epicsEnvSetCallFunc);
    iocshRegister(&epicsParamShowFuncDef,epicsParamShowCallFunc);
    iocshRegister(&epicsEnvShowFuncDef,epicsEnvShowCallFunc);
    iocshRegister(&iocLogInitFuncDef,iocLogInitCallFunc);
    iocshRegister(&iocLogDisableFuncDef,iocLogDisableCallFunc);
    iocshRegister(&iocLogShowFuncDef,iocLogShowCallFunc);
    iocshRegister(&eltcFuncDef,eltcCallFunc);
    iocshRegister(&coreReleaseFuncDef,coreReleaseCallFunc);
}
