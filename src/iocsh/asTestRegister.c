/* asTestRegister.c */
/* Author:  Marty Kraimer Date: 02MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "asDbLib.h"
#include "ioccrf.h"
#define epicsExportSharedSymbols
#include "asTestRegister.h"

/* asSetFilename */
static ioccrfArg asSetFilenameArg0 = { "ascf",ioccrfArgString,0};
static ioccrfArg *asSetFilenameArgs[1] = {&asSetFilenameArg0};
static ioccrfFuncDef asSetFilenameFuncDef =
    {"asSetFilename",1,asSetFilenameArgs};
static void asSetFilenameCallFunc(ioccrfArg **args)
{
    asSetFilename((char *)args[0]->value);
}

/* asSetSubstitutions */
static ioccrfArg asSetSubstitutionsArg0 = { "substitutions",ioccrfArgString,0};
static ioccrfArg *asSetSubstitutionsArgs[1] = {&asSetSubstitutionsArg0};
static ioccrfFuncDef asSetSubstitutionsFuncDef =
    {"asSetSubstitutions",1,asSetSubstitutionsArgs};
static void asSetSubstitutionsCallFunc(ioccrfArg **args)
{
    asSetSubstitutions((char *)args[0]->value);
}

/* asInit */
static ioccrfFuncDef asInitFuncDef = {"asInit",0,0};
static void asInitCallFunc(ioccrfArg **args)
{
    asInit();
}

/* asdbdump */
static ioccrfFuncDef asdbdumpFuncDef = {"asdbdump",0,0};
static void asdbdumpCallFunc(ioccrfArg **args)
{
    asdbdump();
}

/* aspuag */
static ioccrfArg aspuagArg0 = { "uagname",ioccrfArgString,0};
static ioccrfArg *aspuagArgs[1] = {&aspuagArg0};
static ioccrfFuncDef aspuagFuncDef = {"aspuag",1,aspuagArgs};
static void aspuagCallFunc(ioccrfArg **args)
{
    aspuag((char *)args[0]->value);
}

/* asphag */
static ioccrfArg asphagArg0 = { "hagname",ioccrfArgString,0};
static ioccrfArg *asphagArgs[1] = {&asphagArg0};
static ioccrfFuncDef asphagFuncDef = {"asphag",1,asphagArgs};
static void asphagCallFunc(ioccrfArg **args)
{
    asphag((char *)args[0]->value);
}

/* asprules */
static ioccrfArg asprulesArg0 = { "asgname",ioccrfArgString,0};
static ioccrfArg *asprulesArgs[1] = {&asprulesArg0};
static ioccrfFuncDef asprulesFuncDef = {"asprules",1,asprulesArgs};
static void asprulesCallFunc(ioccrfArg **args)
{
    asprules((char *)args[0]->value);
}

/* aspmem */
static ioccrfArg aspmemArg0 = { "asgname",ioccrfArgString,0};
static ioccrfArg aspmemArg1 = { "clients",ioccrfArgInt,0};
static ioccrfArg *aspmemArgs[2] = {&aspmemArg0,&aspmemArg1};
static ioccrfFuncDef aspmemFuncDef = {"aspmem",2,aspmemArgs};
static void aspmemCallFunc(ioccrfArg **args)
{
    aspmem((char *)args[0]->value,*(int *)args[1]->value);
}


void epicsShareAPI asTestRegister(void)
{
    ioccrfRegister(&asSetFilenameFuncDef,asSetFilenameCallFunc);
    ioccrfRegister(&asSetSubstitutionsFuncDef,asSetSubstitutionsCallFunc);
    ioccrfRegister(&asInitFuncDef,asInitCallFunc);
    ioccrfRegister(&asdbdumpFuncDef,asdbdumpCallFunc);
    ioccrfRegister(&aspuagFuncDef,aspuagCallFunc);
    ioccrfRegister(&asphagFuncDef,asphagCallFunc);
    ioccrfRegister(&asprulesFuncDef,asprulesCallFunc);
    ioccrfRegister(&aspmemFuncDef,aspmemCallFunc);
}
