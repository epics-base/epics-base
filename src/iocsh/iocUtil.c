/* iocUtil.c */
/* Author:  W. Eric Norum Date: 02MAY2000 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <logClient.h>

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
    const char * const *argv = args[0].aval.av;
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

/* putenv */
static const ioccrfArg putenvArg0 = { "environment_variable=name",ioccrfArgString};
static const ioccrfArg * const putenvArgs[1] = {&putenvArg0};
static const ioccrfFuncDef putenvFuncDef = {"putenv",1,putenvArgs};
static void putenvCallFunc(const ioccrfArgBuf *args)
{
    char *arg0 = args[0].sval;
    char *cp;

    /*
     * Some versions of putenv set the environment to 
     * point to the string that is passed so we have
     * to make a copy before stashing it.
     * Yes, this will cause memory leaks if the same variable is
     * placed in the environment more than once.
     */
    if (!arg0)
        return;
    cp = calloc(strlen(arg0)+1,sizeof(char));
    strcpy(cp,arg0);
    if ((cp == NULL) || putenv (cp)) {
        free (cp);
        printf ("putenv(%s) failed.\n", args[0].sval);
    }
}

/* env */
static const ioccrfFuncDef showenvFuncDef = {"env",0,NULL};
static void showenvCallFunc(const ioccrfArgBuf *args)
{
    extern char **environ;
    char **sp;

    for (sp = environ ; (sp != NULL) && (*sp != NULL) ; sp++)
        printf ("%s\n", *sp);
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
    ioccrfRegister(&putenvFuncDef,putenvCallFunc);
    ioccrfRegister(&showenvFuncDef,showenvCallFunc);
    ioccrfRegister(&iocLogInitFuncDef,iocLogInitCallFunc);
}
