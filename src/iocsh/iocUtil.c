/* iocUtil.c */
/* Author:  W. Eric Norum Date: 02MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "osiUnistd.h"
#include "osiThread.h"

#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "iocUtilRegister.h"

/* < (runScript) command */
static ioccrfArg runScriptArg0 = { "file name",ioccrfArgString,0};
static ioccrfArg *runScriptArgs[1] = {&runScriptArg0};
static ioccrfFuncDef runScriptFuncDef = {"<",1,runScriptArgs};
static void runScriptCallFunc(ioccrfArg **args)
{
    ioccrf ((char *)args[0]->value);
}

/* chdir */
static ioccrfArg chdirArg0 = { "current directory name",ioccrfArgString,0};
static ioccrfArg *chdirArgs[1] = {&chdirArg0};
static ioccrfFuncDef chdirFuncDef = {"cd",1,chdirArgs};
static void chdirCallFunc(ioccrfArg **args)
{
    int status;
    status = chdir((char *)args[0]->value);
    if (status) {
        printf ("Invalid directory path ignored\n");
    }
}

/* print current working directory */
static ioccrfFuncDef pwdFuncDef = { "pwd", 0, 0 };
static void pwdCallFunc (ioccrfArg **args)
{
    char buf[256];
    char *pwd = getcwd ( buf, sizeof(buf) - 1 );
    if ( pwd ) {
        printf ( "%s\n", pwd );
    }
}

/* show (thread information) */
static ioccrfArg showArg0 = { "task",ioccrfArgString,0};
static ioccrfArg showArg1 = { "task",ioccrfArgString,0};
static ioccrfArg showArg2 = { "task",ioccrfArgString,0};
static ioccrfArg showArg3 = { "task",ioccrfArgString,0};
static ioccrfArg showArg4 = { "task",ioccrfArgString,0};
static ioccrfArg showArg5 = { "task",ioccrfArgString,0};
static ioccrfArg showArg6 = { "task",ioccrfArgString,0};
static ioccrfArg showArg7 = { "task",ioccrfArgString,0};
static ioccrfArg showArg8 = { "task",ioccrfArgString,0};
static ioccrfArg showArg9 = { "task",ioccrfArgString,0};
static ioccrfArg *showArgs[10] = {
    &showArg0,&showArg1,&showArg2,&showArg3,&showArg4,
    &showArg5,&showArg6,&showArg7,&showArg8,&showArg9,
};
static ioccrfFuncDef showFuncDef = {"show",10,showArgs};
static void showCallFunc(ioccrfArg **args)
{
    int i = 0;
    int first = 1;
    int level = 0;
    char *cp;
    threadId tid;
    unsigned long ltmp;
    char *endp;

    if (((cp = (char *)args[i]->value) != NULL)
     && (*cp == '-')) {
        level = atoi (cp + 1);
        i++;
    }
    if ((cp = (char *)args[i]->value) == NULL) {
        threadShowAll (level);
        return;
    }
    for ( ; i < 10 ; i++) {
        if ((cp = (char *)args[i]->value) == NULL)
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
static ioccrfFuncDef threadInitFuncDef =
    {"threadInit",0,0};
static void threadInitCallFunc(ioccrfArg **args)
{
    threadInit();
}

/* putenv */
static ioccrfArg putenvArg0 = { "environment_variable=name",ioccrfArgString,0};
static ioccrfArg *putenvArgs[1] = {&putenvArg0};
static ioccrfFuncDef putenvFuncDef = {"putenv",1,putenvArgs};
static void putenvCallFunc(ioccrfArg **args)
{
    const char *cp = (char *)args[0]->value;

    if (!cp)
        return;
    if (putenv (cp))
        printf ("putenv(%s) failed.\n", cp);
}

void epicsShareAPI iocUtilRegister(void)
{
    ioccrfRegister(&runScriptFuncDef,runScriptCallFunc);
    ioccrfRegister(&chdirFuncDef,chdirCallFunc);
    ioccrfRegister(&pwdFuncDef,pwdCallFunc);
    ioccrfRegister(&showFuncDef,showCallFunc);
    ioccrfRegister(&threadInitFuncDef,threadInitCallFunc);
    ioccrfRegister(&putenvFuncDef,putenvCallFunc);
}
