/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* caTestRegister.c */
/* Author:  Marty Kraimer Date: 01MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsStdio.h"
#include "rsrv.h"
#include "dbEvent.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "caTestRegister.h"

/* casr */
static const iocshArg casrArg0 = { "level",iocshArgInt};
static const iocshArg * const casrArgs[1] = {&casrArg0};
static const iocshFuncDef casrFuncDef = {"casr",1,casrArgs};
static void casrCallFunc(const iocshArgBuf *args)
{
    casr(args[0].ival);
}

/* dbel */
static const iocshArg dbelArg0 = { "record name",iocshArgString};
static const iocshArg dbelArg1 = { "level",iocshArgInt};
static const iocshArg * const dbelArgs[2] = {&dbelArg0,&dbelArg1};
static const iocshFuncDef dbelFuncDef = {"dbel",2,dbelArgs};
static void dbelCallFunc(const iocshArgBuf *args)
{
    dbel(args[0].sval, args[1].ival);
}


void epicsShareAPI caTestRegister(void)
{
    iocshRegister(&casrFuncDef,casrCallFunc);
    iocshRegister(&dbelFuncDef,dbelCallFunc);
}
