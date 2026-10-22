/*************************************************************************\
* Copyright (c) 2007 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "osiSock.h"
#include "iocsh.h"

#include "rsrv.h"
#include "server.h"

void casmatch(const char *pattern, unsigned level);
#include "epicsExport.h"


/* casr */
static const iocshArg casrArg0 = { "level",iocshArgInt};
static const iocshArg * const casrArgs[1] = {&casrArg0};
static const iocshFuncDef casrFuncDef = {"casr",1,casrArgs,
                                         "Channel Access Server Report with following levels:\n"
                                         "  Level 0  - server’s protocol version level and summary for each attached client\n"
                                         "        1  - adds information about connected clients and beacons\n"
                                         "        2  - adds specific channel names and UDP search requests\n"
                                         "        3+ - expert\n"};
static void casrCallFunc(const iocshArgBuf *args)
{
    casr(args[0].ival);
}

/* casmatch */
static const iocshArg casmatchArg0 = { "name-pattern", iocshArgString };
static const iocshArg casmatchArg1 = { "level", iocshArgInt };
static const iocshArg * const casmatchArgs[2] = {&casmatchArg0, &casmatchArg1};
static const iocshFuncDef casmatchFuncDef = {"casmatch", 2, casmatchArgs,
    "Report TCP Channel Access clients with channel names matching name-pattern.\n"
    "  Level 0  - matching client and channel summary\n"
    "        1  - adds matching client information\n"
    "        2  - adds matching channel names\n"
    "        3  - adds channel and access information\n"
    "        4  - adds connection diagnostics\n"
    "        5+ - adds memory and lock diagnostics\n"
    "The pattern uses case-sensitive whole-string * and ? glob matching on the\n"
    "original create-channel name; duplicate channel objects are counted.\n"
    "The diagnostic holds server locks while reporting and can delay CA activity.\n"};
static void casmatchCallFunc(const iocshArgBuf *args)
{
    if (!args[0].sval || !*args[0].sval) {
        printf("Usage: casmatch name-pattern [level]\n");
    } else {
        casmatch(args[0].sval, args[1].ival < 0 ? 0 : args[1].ival);
    }
}

static
void rsrvRegistrar(void)
{
    rsrv_register_server();
    iocshRegister(&casrFuncDef,casrCallFunc);
    iocshRegister(&casmatchFuncDef,casmatchCallFunc);
}

epicsExportAddress(int, CASDEBUG);
epicsExportRegistrar(rsrvRegistrar);
