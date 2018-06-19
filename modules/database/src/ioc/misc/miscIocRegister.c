/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdlib.h>

#include "iocsh.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocInit.h"
#include "epicsExport.h"
#include "epicsRelease.h"
#include "miscIocRegister.h"

/* iocInit */
static const iocshFuncDef iocInitFuncDef = {"iocInit",0,NULL};
static void iocInitCallFunc(const iocshArgBuf *args)
{
    iocInit();
}

/* iocBuild */
static const iocshFuncDef iocBuildFuncDef = {"iocBuild",0,NULL};
static void iocBuildCallFunc(const iocshArgBuf *args)
{
    iocBuild();
}

/* iocRun */
static const iocshFuncDef iocRunFuncDef = {"iocRun",0,NULL};
static void iocRunCallFunc(const iocshArgBuf *args)
{
    iocRun();
}

/* iocPause */
static const iocshFuncDef iocPauseFuncDef = {"iocPause",0,NULL};
static void iocPauseCallFunc(const iocshArgBuf *args)
{
    iocPause();
}

/* coreRelease */
static const iocshFuncDef coreReleaseFuncDef = {"coreRelease",0,NULL};
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
 * Disabled by default, for security reasons. To enable this command, add
 *     registrar(iocshSystemCommand)
 * to an application dbd file.
 */
static const iocshArg systemArg0 = { "command string",iocshArgString};
static const iocshArg * const systemArgs[] = {&systemArg0};
static const iocshFuncDef systemFuncDef = {"system",1,systemArgs};
static void systemCallFunc(const iocshArgBuf *args)
{
    system(args[0].sval);
}

static void iocshSystemCommand(void)
{
    if (system(NULL))
        iocshRegister(&systemFuncDef, systemCallFunc);
    else
        errlogPrintf ("Can't register 'system' command -- no command interpreter available.\n");
}
epicsExportRegistrar(iocshSystemCommand);
