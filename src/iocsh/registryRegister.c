/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* registerRegister.c */
/* Author:  Andrew Johnson Date: 24SEP2002 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "registryFunction.h"

#define epicsExportSharedSymbols
#include "iocsh.h"
#include "registryRegister.h"

static const iocshArg registryXxxFindArg0 = { "name",iocshArgString};
static const iocshArg * const registryXxxFindArgs[1] = {&registryXxxFindArg0};

/* registryRecordTypeFind */
static const iocshFuncDef registryRecordTypeFindFuncDef = {
    "registryRecordTypeFind",1,registryXxxFindArgs};
static void registryRecordTypeFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryRecordTypeFind(args[0].sval));
}

/* registryDeviceSupportFind */
static const iocshFuncDef registryDeviceSupportFindFuncDef = {
    "registryDeviceSupportFind",1,registryXxxFindArgs};
static void registryDeviceSupportFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryDeviceSupportFind(args[0].sval));
}

/* registryDriverSupportFind */
static const iocshFuncDef registryDriverSupportFindFuncDef = {
    "registryDriverSupportFind",1,registryXxxFindArgs};
static void registryDriverSupportFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryDriverSupportFind(args[0].sval));
}

/* registryFunctionFind */
static const iocshFuncDef registryFunctionFindFuncDef = {
    "registryFunctionFind",1,registryXxxFindArgs};
static void registryFunctionFindCallFunc(const iocshArgBuf *args) {
    printf("%p\n", (void*) registryFunctionFind(args[0].sval));
}

void epicsShareAPI registryRegister(void)
{
    iocshRegister(&registryRecordTypeFindFuncDef,registryRecordTypeFindCallFunc);
    iocshRegister(&registryDeviceSupportFindFuncDef,registryDeviceSupportFindCallFunc);
    iocshRegister(&registryDriverSupportFindFuncDef,registryDriverSupportFindCallFunc);
    iocshRegister(&registryFunctionFindFuncDef,registryFunctionFindCallFunc);
}
