/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "envDefs.h"
#include "epicsVersion.h"
#include "iocsh.h"
#include "libComRegister.h"

#include "asIocRegister.h"
#include "dbAccess.h"
#include "dbIocRegister.h"
#include "dbStaticIocRegister.h"
#include "dbtoolsIocRegister.h"
#include "iocshRegisterCommon.h"
#include "miscIocRegister.h"
#include "registryIocRegister.h"

#define quote(v) #v
#define str(v) quote(v)

/* registerAllRecordDeviceDrivers */
static const iocshArg rrddArg0 = {"pdbbase", iocshArgPdbbase};
static const iocshArg *rrddArgs[] = {&rrddArg0};
static const iocshFuncDef rrddFuncDef = {
    "registerAllRecordDeviceDrivers",
    1,
    rrddArgs,
    "Register all records, devices, from all DBD available.\n\n"
    "Calling this function is equivalent to calling every\n"
    "'<name>_registerRecordDeviceDriver' which has been linked\n"
    "into the process, e.g. by dynamic loading, or by linking with\n"
    "a generated '<name>_registerRecordDeviceDriver.cpp' files.\n\n"
    "These are registered into the database given as first argument,\n"
    "which should always be 'pdbbase'.\n\n"
    "Example: registerAllRecordDeviceDrivers pdbbase\n",
};
static void rrddCallFunc(const iocshArgBuf *args)
{
    iocshSetError(registerAllRecordDeviceDrivers(*iocshPpdbbase));
}

void iocshRegisterCommon(void)
{
    const char *targetArch = envGetConfigParamPtr(&EPICS_BUILD_TARGET_ARCH);
    iocshPpdbbase = &pdbbase;

    /* This uses a config param so the user can override it */
    if (targetArch) {
        epicsEnvSet("ARCH", targetArch);
    }

    /* Base build version variables */
    epicsEnvSet("EPICS_VERSION_MAJOR", str(EPICS_VERSION));
    epicsEnvSet("EPICS_VERSION_MIDDLE", str(EPICS_REVISION));
    epicsEnvSet("EPICS_VERSION_MINOR", str(EPICS_MODIFICATION));
    epicsEnvSet("EPICS_VERSION_PATCH", str(EPICS_PATCH_LEVEL));
    epicsEnvSet("EPICS_VERSION_SNAPSHOT", EPICS_DEV_SNAPSHOT);
    epicsEnvSet("EPICS_VERSION_SITE", EPICS_SITE_VERSION);
    epicsEnvSet("EPICS_VERSION_SHORT", EPICS_VERSION_SHORT);
    epicsEnvSet("EPICS_VERSION_FULL", EPICS_VERSION_FULL);

    dbStaticIocRegister();
    registryIocRegister();
    dbIocRegister();
    dbtoolsIocRegister();
    asIocRegister();
    miscIocRegister();
    libComRegister();
    iocshRegister(&rrddFuncDef, rrddCallFunc);
}
