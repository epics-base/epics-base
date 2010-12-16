/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "iocsh.h"

#define epicsExportSharedSymbols
#include "dbtoolsIocRegister.h"
#include "dbLoadTemplate.h"


/* dbLoadTemplate */
static const iocshArg dbLoadTemplateArg0 = { "file name",iocshArgString};
static const iocshArg * const dbLoadTemplateArgs[1] = {&dbLoadTemplateArg0};
static const iocshFuncDef dbLoadTemplateFuncDef =
    {"dbLoadTemplate",1,dbLoadTemplateArgs};
static void dbLoadTemplateCallFunc(const iocshArgBuf *args)
{
    dbLoadTemplate(args[0].sval);
}


void epicsShareAPI dbtoolsIocRegister(void)
{
    iocshRegister(&dbLoadTemplateFuncDef,dbLoadTemplateCallFunc);
}
