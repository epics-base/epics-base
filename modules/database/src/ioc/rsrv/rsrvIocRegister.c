/*************************************************************************\
* Copyright (c) 2007 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "osiSock.h"
#include "iocsh.h"

#define epicsExportSharedSymbols
#include "rsrv.h"
#include "server.h"
#include "epicsExport.h"


/* casr */
static const iocshArg casrArg0 = { "level",iocshArgInt};
static const iocshArg * const casrArgs[1] = {&casrArg0};
static const iocshFuncDef casrFuncDef = {"casr",1,casrArgs};
static void casrCallFunc(const iocshArgBuf *args)
{
    casr(args[0].ival);
}

static
void rsrvRegistrar(void)
{
    rsrv_register_server();
    iocshRegister(&casrFuncDef,casrCallFunc);
}

epicsExportAddress(int, CASDEBUG);
epicsExportRegistrar(rsrvRegistrar);
