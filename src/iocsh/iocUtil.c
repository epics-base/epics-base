/* iocUtil.c */
/* Author:  W. Eric Norum Date: 02MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <logClient.h>

#include "osiUnistd.h"
#include "osiThread.h"

#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "iocUtilRegister.h"

/* < (runScript) command */
static const ioccrfArg runScriptArg0 = { "file name",ioccrfArgString};
static const ioccrfArg *runScriptArgs[1] = {&runScriptArg0};
static const ioccrfFuncDef runScriptFuncDef = {"<",1,runScriptArgs};
static void runScriptCallFunc(const ioccrfArgBuf *args)
{
    ioccrf (args[0].sval);
}

/* chdir */
static const ioccrfArg chdirArg0 = { "current directory name",ioccrfArgString};
static const ioccrfArg *chdirArgs[1] = {&chdirArg0};
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
static const ioccrfArg showArg0 = { "task",ioccrfArgString};
static const ioccrfArg showArg1 = { "task",ioccrfArgString};
static const ioccrfArg showArg2 = { "task",ioccrfArgString};
static const ioccrfArg showArg3 = { "task",ioccrfArgString};
static const ioccrfArg showArg4 = { "task",ioccrfArgString};
static const ioccrfArg showArg5 = { "task",ioccrfArgString};
static const ioccrfArg showArg6 = { "task",ioccrfArgString};
static const ioccrfArg showArg7 = { "task",ioccrfArgString};
static const ioccrfArg showArg8 = { "task",ioccrfArgString};
static const ioccrfArg showArg9 = { "task",ioccrfArgString};
static const ioccrfArg *showArgs[10] = {
    &showArg0,&showArg1,&showArg2,&showArg3,&showArg4,
    &showArg5,&showArg6,&showArg7,&showArg8,&showArg9,
};
static const ioccrfFuncDef showFuncDef = {"show",10,showArgs};
static void showCallFunc(const ioccrfArgBuf *args)
{
    int i = 0;
    int first = 1;
    int level = 0;
    char *cp;
    threadId tid;
    unsigned long ltmp;
    char *endp;

    if (((cp = args[i].sval) != NULL)
     && (*cp == '-')) {
        level = atoi (cp + 1);
        i++;
    }
    if ((cp = args[i].sval) == NULL) {
        threadShowAll (level);
        return;
    }
    for ( ; i < 10 ; i++) {
        if ((cp = args[i].sval) == NULL)
            return;
        ltmp = strtoul (cp, &endp, 16);
        if (*endp) {
            tid = threadGetId (cp);
            if (!tid) {
                printf ("*** argument %d (%s) is not a valid task name ***\n", i+1, cp);
                continue;
            }
        }
        else {
            tid = (threadId)ltmp;
        }
        if (first) {
            threadShow (0, level);
            first = 0;
        }
        threadShow (tid, level);
    }
}

/* threadInit */
static const ioccrfFuncDef threadInitFuncDef =
    {"threadInit",0};
static void threadInitCallFunc(const ioccrfArgBuf *args)
{
    threadInit();
}

/* putenv */
static const ioccrfArg putenvArg0 = { "environment_variable=name",ioccrfArgString};
static const ioccrfArg *putenvArgs[1] = {&putenvArg0};
static const ioccrfFuncDef putenvFuncDef = {"putenv",1,putenvArgs};
static void putenvCallFunc(const ioccrfArgBuf *args)
{
    const char *cp = args[0].sval;
    int putenv(const char *);

    if (!cp)
        return;
    if (putenv (cp))
        printf ("putenv(%s) failed.\n", cp);
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
    ioccrfRegister(&threadInitFuncDef,threadInitCallFunc);
    ioccrfRegister(&putenvFuncDef,putenvCallFunc);
    ioccrfRegister(&iocLogInitFuncDef,iocLogInitCallFunc);
}
