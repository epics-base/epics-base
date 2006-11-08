/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocCoreLimitsRegister.c */
/* Author:  Marty Kraimer Date: 12DEC2002 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsStdio.h"
#include "callback.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbScan.h"
#include "errlog.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "iocCoreLimitsRegister.h"

/* callbackSetQueueSize */
static const iocshArg callbackSetQueueSizeArg0 = { "bufsize",iocshArgInt};
static const iocshArg * const callbackSetQueueSizeArgs[1] =
    {&callbackSetQueueSizeArg0};
static const iocshFuncDef callbackSetQueueSizeFuncDef =
    {"callbackSetQueueSize",1,callbackSetQueueSizeArgs};
static void callbackSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    callbackSetQueueSize(args[0].ival);
}

/* dbPvdTableSize */
static const iocshArg dbPvdTableSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const dbPvdTableSizeArgs[1] =
    {&dbPvdTableSizeArg0};
static const iocshFuncDef dbPvdTableSizeFuncDef =
    {"dbPvdTableSize",1,dbPvdTableSizeArgs};
static void dbPvdTableSizeCallFunc(const iocshArgBuf *args)
{
    dbPvdTableSize(args[0].ival);
}

/* scanOnceSetQueueSize */
static const iocshArg scanOnceSetQueueSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const scanOnceSetQueueSizeArgs[1] =
    {&scanOnceSetQueueSizeArg0};
static const iocshFuncDef scanOnceSetQueueSizeFuncDef =
    {"scanOnceSetQueueSize",1,scanOnceSetQueueSizeArgs};
static void scanOnceSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    scanOnceSetQueueSize(args[0].ival);
}

/* errlogInit */
static const iocshArg errlogInitArg0 = { "bufsize",iocshArgInt};
static const iocshArg * const errlogInitArgs[1] = {&errlogInitArg0};
static const iocshFuncDef errlogInitFuncDef =
    {"errlogInit",1,errlogInitArgs};
static void errlogInitCallFunc(const iocshArgBuf *args)
{
    errlogInit(args[0].ival);
}

/* errlogInit2 */
static const iocshArg errlogInit2Arg0 = { "bufSize",iocshArgInt};
static const iocshArg errlogInit2Arg1 = { "maxMsgSize",iocshArgInt};
static const iocshArg * const errlogInit2Args[] = 
    {&errlogInit2Arg0, &errlogInit2Arg1};
static const iocshFuncDef errlogInit2FuncDef =
    {"errlogInit2", 2, errlogInit2Args};
static void errlogInit2CallFunc(const iocshArgBuf *args)
{
    errlogInit2(args[0].ival, args[1].ival);
}

void epicsShareAPI iocCoreLimitsRegister(void)
{
    iocshRegister(&callbackSetQueueSizeFuncDef,callbackSetQueueSizeCallFunc);
    iocshRegister(&dbPvdTableSizeFuncDef,dbPvdTableSizeCallFunc);
    iocshRegister(&scanOnceSetQueueSizeFuncDef,scanOnceSetQueueSizeCallFunc);
    iocshRegister(&errlogInitFuncDef,errlogInitCallFunc);
    iocshRegister(&errlogInit2FuncDef,errlogInit2CallFunc);
}
