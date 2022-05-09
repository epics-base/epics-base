/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "iocsh.h"

#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "registryFunction.h"
#include "registryIocRegister.h"
#include "registryRecordType.h"

static const iocshArg registryXxxFindArg0 = { "name",iocshArgString};
static const iocshArg * const registryXxxFindArgs[1] = {&registryXxxFindArg0};

/* registryRecordTypeFind */
static const iocshFuncDef registryRecordTypeFindFuncDef = {
    "registryRecordTypeFind",
    1,
    registryXxxFindArgs,
    "Prints the registry address of the record type given as first argument.\n\n"
    "Example: registryRecordTypeFind ai\n",
};
static void registryRecordTypeFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryRecordTypeFind(args[0].sval));
}

/* registryDeviceSupportFind */
static const iocshFuncDef registryDeviceSupportFindFuncDef = {
    "registryDeviceSupportFind",
    1,
    registryXxxFindArgs,
    "Prints the registry address of the device support given as first argument.\n\n"
    "Example: registryDeviceSupportFind devAaiSoft\n",
};
static void registryDeviceSupportFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryDeviceSupportFind(args[0].sval));
}

/* registryDriverSupportFind */
static const iocshFuncDef registryDriverSupportFindFuncDef = {
    "registryDriverSupportFind",
    1,
    registryXxxFindArgs,
    "Prints the registry address of the driver support given as first argument.\n\n"
    "Example: registryDriverSupportFind stream\n",
};
static void registryDriverSupportFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryDriverSupportFind(args[0].sval));
}

/* registryFunctionFind */
static const iocshFuncDef registryFunctionFindFuncDef = {
    "registryFunctionFind",
    1,
    registryXxxFindArgs,
    "Prints the registry address of the registered function given as first argument.\n\n"
    "Example: registryFunctionFind registryFunctionFind\n",
};
static void registryFunctionFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryFunctionFind(args[0].sval));
}

void registryIocRegister(void)
{
    iocshRegister(&registryRecordTypeFindFuncDef,registryRecordTypeFindCallFunc);
    iocshRegister(&registryDeviceSupportFindFuncDef,registryDeviceSupportFindCallFunc);
    iocshRegister(&registryDriverSupportFindFuncDef,registryDriverSupportFindCallFunc);
    iocshRegister(&registryFunctionFindFuncDef,registryFunctionFindCallFunc);
}
