/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Escape to system command interpreter.  To enable this command, add
 *      registrar(iocshSystemCommand)
 * to an application dbd file.
 *
 * $Id$
 */
#include <stdlib.h>
#include <errlog.h>
#include <epicsExport.h>

#define epicsExportSharedSymbols
#include <iocsh.h>

/* system */
static const iocshArg systemArg0 = { "command string",iocshArgString};
static const iocshArg * const systemArgs[1] = {&systemArg0};
static const iocshFuncDef systemFuncDef = {"system",1,systemArgs};
static void systemCallFunc(const iocshArgBuf *args)
{
    system(args[0].sval);
}

static void
iocshSystemCommand(void)
{
    if (system(NULL))
        iocshRegister(&systemFuncDef,systemCallFunc);
    else
        errlogPrintf ("Can't register 'system' command -- no command interpreter available.\n");
}
epicsExportRegistrar(iocshSystemCommand);
