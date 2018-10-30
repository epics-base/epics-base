/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "epicsFindSymbol.h"
#include "iocsh.h"
#include "epicsExport.h"

IOCSH_STATIC_FUNC void dlload(const char* name)
{
    if (!epicsLoadLibrary(name)) {
        printf("epicsLoadLibrary failed: %s\n", epicsLoadError());
    }
}

static const iocshArg dlloadArg0 = { "path/library.so", iocshArgString};
static const iocshArg * const dlloadArgs[] = {&dlloadArg0};
static const iocshFuncDef dlloadFuncDef = {"dlload", 1, dlloadArgs};
static void dlloadCallFunc(const iocshArgBuf *args)
{
    dlload(args[0].sval);
}

static void dlloadRegistar(void) {
    iocshRegister(&dlloadFuncDef, dlloadCallFunc);
}
epicsExportRegistrar(dlloadRegistar);
