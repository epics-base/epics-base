/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "epicsFindSymbol.h"
#include "iocsh.h"
#include "epicsExport.h"

static const iocshArg dlloadArg0 = { "path/library.so", iocshArgString};
static const iocshArg * const dlloadArgs[] = {&dlloadArg0};
static const iocshFuncDef dlloadFuncDef = {"dlload", 1, dlloadArgs};
static void dlloadCallFunc(const iocshArgBuf *args)
{
    if (!epicsLoadLibrary(args[0].sval)) {
        printf("epicsLoadLibrary failed: %s\n", epicsLoadError());
    }
}

static void dlloadRegistar(void) {
    iocshRegister(&dlloadFuncDef, dlloadCallFunc);
}
epicsExportRegistrar(dlloadRegistar);
