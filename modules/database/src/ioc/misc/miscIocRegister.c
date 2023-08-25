/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>

#include "iocsh.h"
#include "errlog.h"

#include "iocInit.h"
#include "epicsExport.h"
#include "epicsRelease.h"
#include "miscIocRegister.h"

/* iocInit */
static const iocshFuncDef iocInitFuncDef = {"iocInit",0,NULL,
             "Initializes the various epics components and starts the IOC running.\n"};
static void iocInitCallFunc(const iocshArgBuf *args)
{
    iocshSetError(iocInit());
}

/* iocBuild */
static const iocshFuncDef iocBuildFuncDef = {"iocBuild",0,NULL,
             "First step of the IOC initialization, puts the IOC into a ready-to-run (quiescent) state.\n"
             "Needs iocRun() to make the IOC live.\n"};
static void iocBuildCallFunc(const iocshArgBuf *args)
{
    iocshSetError(iocBuild());
}

/* iocRun */
static const iocshFuncDef iocRunFuncDef = {"iocRun",0,NULL, 
             "Bring the IOC out of its initial quiescent state to the running state.\n"
             "See more: iocBuild, iocPause\n"};
static void iocRunCallFunc(const iocshArgBuf *args)
{
    iocshSetError(iocRun());
}

/* iocPause */
static const iocshFuncDef iocPauseFuncDef = {"iocPause",0,NULL,
             "Brings a running IOC to a quiescent state with all record processing frozen.\n"
             "See more: iocBuild, iocRub, iocInit\n"};
static void iocPauseCallFunc(const iocshArgBuf *args)
{
    iocshSetError(iocPause());
}

/* coreRelease */
static const iocshFuncDef coreReleaseFuncDef = {"coreRelease",0,NULL,
             "Print release information for iocCore.\n"};
static void coreReleaseCallFunc(const iocshArgBuf *args)
{
    coreRelease ();
}


void miscIocRegister(void)
{
    iocshRegister(&iocInitFuncDef,iocInitCallFunc);
    iocshRegister(&iocBuildFuncDef,iocBuildCallFunc);
    iocshRegister(&iocRunFuncDef,iocRunCallFunc);
    iocshRegister(&iocPauseFuncDef,iocPauseCallFunc);
    iocshRegister(&coreReleaseFuncDef, coreReleaseCallFunc);
}


/* system -- escape to system command interpreter.
 *
 * Disabled by default for security reasons, not available on all OSs.
 * To enable this command, add
 *     registrar(iocshSystemCommand)
 * to an application dbd file, or include system.dbd
 */
#ifndef SYSTEM_UNAVAILABLE
static const iocshArg systemArg0 = { "command string",iocshArgString};
static const iocshArg * const systemArgs[] = {&systemArg0};
static const iocshFuncDef systemFuncDef = {"system",1,systemArgs,
             "Send command string to the system command interpreter for execution.\n"
             "Not available on all OSs.\n"
             "To enable this command, add registrar(iocshSystemCommand) to an application dbd file,\n" 
             "or include system.dbd\n"};
static void systemCallFunc(const iocshArgBuf *args)
{
    iocshSetError(system(args[0].sval));
}
#endif

static void iocshSystemCommand(void)
{
#ifndef SYSTEM_UNAVAILABLE
    if (system(NULL))
        iocshRegister(&systemFuncDef, systemCallFunc);
    else
#endif
        errlogPrintf ("Can't register 'system' command -- no command interpreter available.\n");
}
epicsExportRegistrar(iocshSystemCommand);
