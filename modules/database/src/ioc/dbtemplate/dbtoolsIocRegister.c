/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "iocsh.h"

#include "dbtoolsIocRegister.h"
#include "dbLoadTemplate.h"


/* dbLoadTemplate */
static const iocshArg dbLoadTemplateArg0 = {"filename", iocshArgStringPath};
static const iocshArg dbLoadTemplateArg1 = {"var1=value1,var2=value2", iocshArgString};
static const iocshArg * const dbLoadTemplateArgs[2] = {
    &dbLoadTemplateArg0, &dbLoadTemplateArg1
};
static const iocshFuncDef dbLoadTemplateFuncDef = {
    "dbLoadTemplate",
    2,
    dbLoadTemplateArgs,
    "Load the substitution file given as first argument, apply the substitutions\n"
    "for each template in the substitution file, and load them using 'dbLoadRecords'.\n\n"
    "The second argument provides extra variables to substitute in the\n"
    "template files (not the substitution file).\n\n"
    "See 'help dbLoadRecords' for more information.\n\n"
    "Example: dbLoadTemplate db/my.substitutions 'user=myself,host=myhost'\n",
};
static void dbLoadTemplateCallFunc(const iocshArgBuf *args)
{
    iocshSetError(dbLoadTemplate(args[0].sval, args[1].sval));
}


void dbtoolsIocRegister(void)
{
    iocshRegister(&dbLoadTemplateFuncDef, dbLoadTemplateCallFunc);
}
