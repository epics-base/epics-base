/* iocUtil.c */
/* Author:  W. Eric Norum Date: 02MAY2000 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <logClient.h>
#include <envDefs.h>

#include "osiUnistd.h"
#include "epicsThread.h"

#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "iocUtilRegister.h"

/* < (runScript) command */
static const ioccrfArg runScriptArg0 = { "command file name",ioccrfArgString};
static const ioccrfArg * const runScriptArgs[1] = {&runScriptArg0};
static const ioccrfFuncDef runScriptFuncDef = {"<",1,runScriptArgs};
static void runScriptCallFunc(const ioccrfArgBuf *args)
{
    ioccrf (args[0].sval);
}

/* chdir */
static const ioccrfArg chdirArg0 = { "current directory name",ioccrfArgString};
static const ioccrfArg * const chdirArgs[1] = {&chdirArg0};
static const ioccrfFuncDef chdirFuncDef = {"cd",1,chdirArgs};
static void chdirCallFunc(const ioccrfArgBuf *args)
{
    int status;
    status = chdir(args[0].sval);
    if (status) {
        printf ("Invalid directory path ignored\n");
    }
}

/* print current working directory */
static const ioccrfFuncDef pwdFuncDef = { "pwd", 0, 0 };
static void pwdCallFunc (const ioccrfArgBuf *args)
{
    char buf[256];
    char *pwd = getcwd ( buf, sizeof(buf) - 1 );
    if ( pwd ) {
        printf ( "%s\n", pwd );
    }
}

/* show (thread information) */
static const ioccrfArg showArg0 = { "[-level] [task ...]", ioccrfArgArgv};
static const ioccrfArg * const showArgs[1] = { &showArg0 };
static const ioccrfFuncDef showFuncDef = {"show",1,showArgs};
static void showCallFunc(const ioccrfArgBuf *args)
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
        ltmp = strtoul (cp, &endp, 16);
        if (*endp) {
            tid = epicsThreadGetId (cp);
            if (!tid) {
                printf ("*** argument %d (%s) is not a valid task name ***\n", i, cp);
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
static const ioccrfArg epicsEnvSetArg0 = { "name",ioccrfArgString};
static const ioccrfArg epicsEnvSetArg1 = { "value",ioccrfArgString};
static const ioccrfArg * const epicsEnvSetArgs[2] = {&epicsEnvSetArg0,&epicsEnvSetArg1};
static const ioccrfFuncDef epicsEnvSetFuncDef = {"epicsEnvSet",2,epicsEnvSetArgs};
static void epicsEnvSetCallFunc(const ioccrfArgBuf *args)
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
static const ioccrfFuncDef epicsParamShowFuncDef = {"epicsParamShow",0,NULL};
static void epicsParamShowCallFunc(const ioccrfArgBuf *args)
{
    epicsPrtEnvParams ();
}

/* epicsEnvShow */
static const ioccrfArg epicsEnvShowArg0 = { "[name ...]", ioccrfArgArgv};
static const ioccrfArg * const epicsEnvShowArgs[1] = {&epicsEnvShowArg0};
static const ioccrfFuncDef epicsEnvShowFuncDef = {"epicsEnvShow",1,epicsEnvShowArgs};
static void epicsEnvShowCallFunc(const ioccrfArgBuf *args)
{
    epicsEnvShow (args[0].aval.ac, args[0].aval.av);
}

/* iocLogInit */
static const ioccrfFuncDef iocLogInitFuncDef = {"iocLogInit",0};
static void iocLogInitCallFunc(const ioccrfArgBuf *args)
{
    iocLogInit ();
}

void epicsShareAPI iocUtilRegister(void)
{
    ioccrfRegister(&runScriptFuncDef,runScriptCallFunc);
    ioccrfRegister(&chdirFuncDef,chdirCallFunc);
    ioccrfRegister(&pwdFuncDef,pwdCallFunc);
    ioccrfRegister(&showFuncDef,showCallFunc);
    ioccrfRegister(&epicsEnvSetFuncDef,epicsEnvSetCallFunc);
    ioccrfRegister(&epicsParamShowFuncDef,epicsParamShowCallFunc);
    ioccrfRegister(&epicsEnvShowFuncDef,epicsEnvShowCallFunc);
    ioccrfRegister(&iocLogInitFuncDef,iocLogInitCallFunc);
}
