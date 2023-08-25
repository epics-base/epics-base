/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>

#include "iocsh.h"
#include "asLib.h"
#include "epicsStdioRedirect.h"
#include "epicsString.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "envDefs.h"
#include "osiUnistd.h"
#include "logClient.h"
#include "errlog.h"
#include "taskwd.h"
#include "registry.h"
#include "epicsGeneralTime.h"
#include "freeList.h"
#include "libComRegister.h"

/* Register the PWD environment variable when the cd IOC shell function is
 * registered. This variable contains the current directory path.
 */
static void updatePWD() {
    static int lasterror;
    char buf[1024];
    char *pwd = getcwd(buf, sizeof(buf));
    if (pwd) {
        pwd[sizeof(buf) - 1] = '\0';
        lasterror = 0;
        epicsEnvSet("PWD", buf);
    } else {
        if(lasterror!=errno) {
            lasterror = errno;
            if (errno == ERANGE) {
                fprintf(stderr, "Warning: Current path exceeds %u characters\n",
                                                              (unsigned)sizeof(buf));
            } else {
                perror("getcwd");
            }
            fprintf(stderr, "Warning: Unable to update $PWD\n");
        }
    }
}

/* date */
void date(const char *format)
{
    epicsTimeStamp now;
    char nowText[80] = {'\0'};

    if (epicsTimeGetCurrent(&now)) {
        puts("Current time not available.");
        return;
    }
    if (format == NULL || format[0] == '\0')
        format = "%Y/%m/%d %H:%M:%S.%06f";
    epicsTimeToStrftime(nowText, sizeof(nowText), format, &now);
    puts(nowText);
}

static const iocshArg dateArg0 = { "format",iocshArgString};
static const iocshArg * const dateArgs[] = {&dateArg0};
static const iocshFuncDef dateFuncDef = {"date", 1, dateArgs,
                                         "Print current date and time\n"
                                         "  (default) - '%Y/%m/%d %H:%M:%S.%06f'\n"};
static void dateCallFunc (const iocshArgBuf *args)
{
    date(args[0].sval);
}

/* echo */
IOCSH_STATIC_FUNC void echo(char* str)
{
    if (str)
        dbTranslateEscape(str, str); /* in-place is safe */
    else
        str = "";
    printf("%s\n", str);
}

static const iocshArg echoArg0 = { "string",iocshArgString};
static const iocshArg * const echoArgs[1] = {&echoArg0};
static const iocshFuncDef echoFuncDef = {"echo",1,echoArgs,
                                         "Print string after expanding macros and environment variables\n"};
static void echoCallFunc(const iocshArgBuf *args)
{
    echo(args[0].sval);
}

/* chdir */
static const iocshArg chdirArg0 = { "directory name",iocshArgStringPath};
static const iocshArg * const chdirArgs[1] = {&chdirArg0};
static const iocshFuncDef chdirFuncDef = {"cd",1,chdirArgs,
                                          "Change directory to new directory provided as parameter\n"};
static void chdirCallFunc(const iocshArgBuf *args)
{
    if (args[0].sval == NULL ||
        iocshSetError(chdir(args[0].sval))) {
        fprintf(stderr, "Invalid directory path, ignored\n");
    } else {
        updatePWD();
    }
}

/* print current working directory */
static const iocshFuncDef pwdFuncDef = {"pwd", 0, 0,
                                        "Print name of current/working directory\n"};
static void pwdCallFunc (const iocshArgBuf *args)
{
    char buf[1024];
    char *pwd = getcwd ( buf, sizeof(buf) );
    if ( pwd ) {
        buf[sizeof(buf)-1u] = '\0';
        printf ( "%s\n", pwd );
    }
}

/* epicsEnvSet */
static const iocshArg epicsEnvSetArg0 = { "name",iocshArgString};
static const iocshArg epicsEnvSetArg1 = { "value",iocshArgString};
static const iocshArg * const epicsEnvSetArgs[2] = {&epicsEnvSetArg0,&epicsEnvSetArg1};
static const iocshFuncDef epicsEnvSetFuncDef = {"epicsEnvSet",2,epicsEnvSetArgs,
                                                "Set environment variable name to value\n"};
static void epicsEnvSetCallFunc(const iocshArgBuf *args)
{
    char *name = args[0].sval;
    char *value = args[1].sval;

    if (name == NULL) {
        fprintf(stderr, "Missing environment variable name argument.\n");
        return;
    }
    if (value == NULL) {
        fprintf(stderr, "Missing environment variable value argument.\n");
        return;
    }
    epicsEnvSet (name, value);
}

/* epicsEnvUnset */
static const iocshArg epicsEnvUnsetArg0 = { "name",iocshArgString};
static const iocshArg * const epicsEnvUnsetArgs[1] = {&epicsEnvUnsetArg0};
static const iocshFuncDef epicsEnvUnsetFuncDef = {"epicsEnvUnset",1,epicsEnvUnsetArgs,
                                                  "Remove variable name from the environment\n"};
static void epicsEnvUnsetCallFunc(const iocshArgBuf *args)
{
    char *name = args[0].sval;

    if (name == NULL) {
        fprintf(stderr, "Missing environment variable name argument.\n");
        return;
    }
    epicsEnvUnset (name);
}

/* epicsParamShow */
IOCSH_STATIC_FUNC void epicsParamShow()
{
    epicsPrtEnvParams ();
}

static const iocshFuncDef epicsParamShowFuncDef = {"epicsParamShow",0,NULL,
                                                   "Show the environment variable parameters used by iocCore\n"};
static void epicsParamShowCallFunc(const iocshArgBuf *args)
{
    epicsParamShow ();
}

/* epicsPrtEnvParams */
static const iocshFuncDef epicsPrtEnvParamsFuncDef = {"epicsPrtEnvParams",0,0,
                                                      "Show the environment variable parameters used by iocCore\n"};
static void epicsPrtEnvParamsCallFunc(const iocshArgBuf *args)
{
    epicsPrtEnvParams ();
}

/* epicsEnvShow */
static const iocshArg epicsEnvShowArg0 = { "[name]",iocshArgString};
static const iocshArg * const epicsEnvShowArgs[1] = {&epicsEnvShowArg0};
static const iocshFuncDef epicsEnvShowFuncDef = {"epicsEnvShow",1,epicsEnvShowArgs,
                                                 "Show environment variables on your system\n"
                                                 "  (default) - show all environment variables\n"
                                                 "   name     - show value of specific environment variable\n"
                                                 "Example: epicsEnvShow\n"
                                                 "Example: epicsEnvShow PATH"};
static void epicsEnvShowCallFunc(const iocshArgBuf *args)
{
    epicsEnvShow (args[0].sval);
}

/* registryDump */
static const iocshFuncDef registryDumpFuncDef = {"registryDump",0,NULL,
                                                 "Dump a hash table of EPICS registry\n"};
static void registryDumpCallFunc(const iocshArgBuf *args)
{
    registryDump ();
}

/* iocLogInit */
static const iocshFuncDef iocLogInitFuncDef = {"iocLogInit",0,0,
                                               "Initialize IOC logging\n"
                                               "  * EPICS environment variable 'EPICS_IOC_LOG_INET' has to be defined\n"
                                               "  * Logging controled via 'iocLogDisable' variable\n"
                                               "       see 'setIocLogDisable' command\n"};
static void iocLogInitCallFunc(const iocshArgBuf *args)
{
    iocLogInit ();
}

/* iocLogDisable */
IOCSH_STATIC_FUNC void setIocLogDisable(int val)
{
    iocLogDisable = val;
}

static const iocshArg iocLogDisableArg0 = {"(0,1)=>(false,true)",iocshArgInt};
static const iocshArg * const iocLogDisableArgs[1] = {&iocLogDisableArg0};
static const iocshFuncDef iocLogDisableFuncDef = {"setIocLogDisable",1,iocLogDisableArgs,
                                                  "Controls the 'iocLogDisable' variable\n"
                                                  "  0 - enable logging\n"
                                                  "  1 - disable logging\n"};
static void iocLogDisableCallFunc(const iocshArgBuf *args)
{
    setIocLogDisable(args[0].ival);
}

/* iocLogShow */
static const iocshArg iocLogShowArg0 = {"level",iocshArgInt};
static const iocshArg * const iocLogShowArgs[1] = {&iocLogShowArg0};
static const iocshFuncDef iocLogShowFuncDef = {"iocLogShow",1,iocLogShowArgs,
                                               "Determine if a IOC Log Prefix has been set\n"};
static void iocLogShowCallFunc(const iocshArgBuf *args)
{
    iocLogShow (args[0].ival);
}

/* eltc */
static const iocshArg eltcArg0 = {"(0,1)=>(false,true)",iocshArgInt};
static const iocshArg * const eltcArgs[1] = {&eltcArg0};
static const iocshFuncDef eltcFuncDef = {"eltc",1,eltcArgs,
                                         "Control display of error log messages on console\n"
                                         "  0 - no\n"
                                         "  1 - yes (default)\n"};
static void eltcCallFunc(const iocshArgBuf *args)
{
    eltc(args[0].ival);
}

/* errlogInit */
static const iocshArg errlogInitArg0 = { "bufSize",iocshArgInt};
static const iocshArg * const errlogInitArgs[1] = {&errlogInitArg0};
static const iocshFuncDef errlogInitFuncDef = {
    "errlogInit",1,errlogInitArgs,
    "Initialize error log client buffer size\n"
    "  bufSize - size of circular buffer (default = 1280 bytes)\n"
};
static void errlogInitCallFunc(const iocshArgBuf *args)
{
    errlogInit(args[0].ival);
}

/* errlogInit2 */
static const iocshArg errlogInit2Arg0 = { "bufSize",iocshArgInt};
static const iocshArg errlogInit2Arg1 = { "maxMsgSize",iocshArgInt};
static const iocshArg * const errlogInit2Args[] =
    {&errlogInit2Arg0, &errlogInit2Arg1};
static const iocshFuncDef errlogInit2FuncDef = {
    "errlogInit2", 2, errlogInit2Args,
    "Initialize error log client buffer size and maximum message size\n"
    "  bufSize    - size of circular buffer       (default = 1280 bytes)\n"
    "  maxMsgSize - maximum size of error message (default =  256 bytes)\n"
};
static void errlogInit2CallFunc(const iocshArgBuf *args)
{
    errlogInit2(args[0].ival, args[1].ival);
}

/* errlog */
IOCSH_STATIC_FUNC void errlog(const char *message)
{
    errlogPrintfNoConsole("%s\n", message);
}

static const iocshArg errlogArg0 = { "message",iocshArgString};
static const iocshArg * const errlogArgs[1] = {&errlogArg0};
static const iocshFuncDef errlogFuncDef = {"errlog",1,errlogArgs,
                                           "Send message to errlog\n"};
static void errlogCallFunc(const iocshArgBuf *args)
{
    errlog(args[0].sval);
    errlogFlush();
}

/* iocLogPrefix */
static const iocshArg iocLogPrefixArg0 = { "prefix",iocshArgString};
static const iocshArg * const iocLogPrefixArgs[1] = {&iocLogPrefixArg0};
static const iocshFuncDef iocLogPrefixFuncDef = {"iocLogPrefix",1,iocLogPrefixArgs,
                                                 "Create the prefix for all messages going into IOC log\n"};
static void iocLogPrefixCallFunc(const iocshArgBuf *args)
{
    iocLogPrefix(args[0].sval);
}

/* epicsThreadShowAll */
static const iocshArg epicsThreadShowAllArg0 = { "level",iocshArgInt};
static const iocshArg * const epicsThreadShowAllArgs[1] = {&epicsThreadShowAllArg0};
static const iocshFuncDef epicsThreadShowAllFuncDef = {"epicsThreadShowAll",1,epicsThreadShowAllArgs,
                                                       "Display info about all threads\n"};
static void epicsThreadShowAllCallFunc(const iocshArgBuf *args)
{
    epicsThreadShowAll(args[0].ival);
}

/* epicsThreadShow */
static const iocshArg threadArg0 = { "[-level] [thread ...]", iocshArgArgv};
static const iocshArg * const threadArgs[1] = { &threadArg0 };
static const iocshFuncDef threadFuncDef = {"epicsThreadShow",1,threadArgs,
                                           "Display info about the specified thread\n"};
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
                fprintf(stderr, "\t'%s' is not a known thread name\n", cp);
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

/* taskwdShow */
static const iocshArg taskwdShowArg0 = { "level",iocshArgInt};
static const iocshArg * const taskwdShowArgs[1] = {&taskwdShowArg0};
static const iocshFuncDef taskwdShowFuncDef = {"taskwdShow",1,taskwdShowArgs,
                                               "Show number of tasks and monitors registered\n"};
static void taskwdShowCallFunc(const iocshArgBuf *args)
{
    taskwdShow(args[0].ival);
}

/* epicsMutexShowAll */
static const iocshArg epicsMutexShowAllArg0 = { "onlyLocked",iocshArgInt};
static const iocshArg epicsMutexShowAllArg1 = { "level",iocshArgInt};
static const iocshArg * const epicsMutexShowAllArgs[2] =
    {&epicsMutexShowAllArg0,&epicsMutexShowAllArg1};
static const iocshFuncDef epicsMutexShowAllFuncDef = {
    "epicsMutexShowAll",2,epicsMutexShowAllArgs,
    "Display information about all epicsMutex semaphores\n"
    "  onlyLocked - non-zero to show only locked semaphores\n"
    "  level      - desired information level to report\n"
};
static void epicsMutexShowAllCallFunc(const iocshArgBuf *args)
{
    epicsMutexShowAll(args[0].ival,args[1].ival);
}

/* epicsThreadSleep */
static const iocshArg epicsThreadSleepArg0 = { "seconds",iocshArgDouble};
static const iocshArg * const epicsThreadSleepArgs[1] = {&epicsThreadSleepArg0};
static const iocshFuncDef epicsThreadSleepFuncDef = {"epicsThreadSleep",1,epicsThreadSleepArgs,
                                                     "Pause execution of IOC shell for <seconds> seconds\n"};
static void epicsThreadSleepCallFunc(const iocshArgBuf *args)
{
    epicsThreadSleep(args[0].dval);
}

/* epicsThreadResume */
static const iocshArg epicsThreadResumeArg0 = { "[thread ...]", iocshArgArgv};
static const iocshArg * const epicsThreadResumeArgs[1] = { &epicsThreadResumeArg0 };
static const iocshFuncDef epicsThreadResumeFuncDef = {"epicsThreadResume",1,epicsThreadResumeArgs,
                                                      "Resume a suspended thread.\n"
                                                      "Only do this if you know that it is safe to "
                                                      "resume a suspended thread\n"};
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
                fprintf(stderr, "'%s' is not a valid thread name\n", cp);
                continue;
            }
        }
        else {
            tid =(epicsThreadId)ltmp;
            epicsThreadGetName(tid, nameBuf, sizeof nameBuf);
            if (nameBuf[0] == '\0') {
                fprintf(stderr, "'%s' is not a valid thread id\n", cp);
                continue;
            }

        }
        if (!epicsThreadIsSuspended(tid)) {
            fprintf(stderr, "Thread %s is not suspended\n", cp);
            continue;
        }
        epicsThreadResume(tid);
    }
}

/* generalTimeReport */
static const iocshArg generalTimeReportArg0 = { "interest_level", iocshArgInt};
static const iocshArg * const generalTimeReportArgs[1] = { &generalTimeReportArg0 };
static const iocshFuncDef generalTimeReportFuncDef = {"generalTimeReport",1,generalTimeReportArgs,
                                                      "Display time providers information for given interest level.\n"
                                                      "interest level 0 - List providers and their priorities.\n"
                                                      "               1 - Additionally show current time obtained from each provider.\n"};
static void generalTimeReportCallFunc(const iocshArgBuf *args)
{
    generalTimeReport(args[0].ival);
}

/* installLastResortEventProvider */
static const iocshFuncDef installLastResortEventProviderFuncDef = {"installLastResortEventProvider",0,NULL,
                                                                   "Installs the optional Last Resort event provider at priority 999,\n"
                                                                   "which returns the current time for every event number\n"};
static void installLastResortEventProviderCallFunc(const iocshArgBuf *args)
{
    installLastResortEventProvider();
}

static iocshVarDef comDefs[] = {
    { "asCheckClientIP", iocshArgInt, 0 },
    { "freeListBypass", iocshArgInt, 0 },
    { NULL, iocshArgInt, NULL }
};

void epicsStdCall libComRegister(void)
{
    iocshRegister(&dateFuncDef, dateCallFunc);
    iocshRegister(&echoFuncDef, echoCallFunc);
    iocshRegister(&chdirFuncDef, chdirCallFunc);
    iocshRegister(&pwdFuncDef, pwdCallFunc);

    updatePWD();

    iocshRegister(&epicsEnvSetFuncDef, epicsEnvSetCallFunc);
    iocshRegister(&epicsEnvUnsetFuncDef, epicsEnvUnsetCallFunc);
    iocshRegister(&epicsParamShowFuncDef, epicsParamShowCallFunc);
    iocshRegister(&epicsPrtEnvParamsFuncDef, epicsPrtEnvParamsCallFunc);
    iocshRegister(&epicsEnvShowFuncDef, epicsEnvShowCallFunc);
    iocshRegister(&registryDumpFuncDef, registryDumpCallFunc);

    iocshRegister(&iocLogInitFuncDef, iocLogInitCallFunc);
    iocshRegister(&iocLogDisableFuncDef, iocLogDisableCallFunc);
    iocshRegister(&iocLogShowFuncDef, iocLogShowCallFunc);
    iocshRegister(&eltcFuncDef, eltcCallFunc);
    iocshRegister(&errlogInitFuncDef,errlogInitCallFunc);
    iocshRegister(&errlogInit2FuncDef,errlogInit2CallFunc);
    iocshRegister(&errlogFuncDef, errlogCallFunc);
    iocshRegister(&iocLogPrefixFuncDef, iocLogPrefixCallFunc);

    iocshRegister(&epicsThreadShowAllFuncDef,epicsThreadShowAllCallFunc);
    iocshRegister(&threadFuncDef, threadCallFunc);
    iocshRegister(&taskwdShowFuncDef,taskwdShowCallFunc);
    iocshRegister(&epicsMutexShowAllFuncDef,epicsMutexShowAllCallFunc);
    iocshRegister(&epicsThreadSleepFuncDef,epicsThreadSleepCallFunc);
    iocshRegister(&epicsThreadResumeFuncDef,epicsThreadResumeCallFunc);

    iocshRegister(&generalTimeReportFuncDef,generalTimeReportCallFunc);
    iocshRegister(&installLastResortEventProviderFuncDef, installLastResortEventProviderCallFunc);

    comDefs[0].pval = &asCheckClientIP;
    comDefs[1].pval = &freeListBypass;
    iocshRegisterVariable(comDefs);
}
