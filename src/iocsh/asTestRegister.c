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

#include "ioccrf.h"
#include "asDbLib.h"
#define epicsExportSharedSymbols
#include "asTestRegister.h"

/* asSetFilename */
ioccrfArg asSetFilenameArg0 = { "ascf",ioccrfArgString,0};
ioccrfArg *asSetFilenameArgs[1] = {&asSetFilenameArg0};
ioccrfFuncDef asSetFilenameFuncDef = {"asSetFilename",1,asSetFilenameArgs};
void asSetFilenameCallFunc(ioccrfArg **args)
{
    asSetFilename((char *)args[0]->value);
}

/* asSetSubstitutions */
ioccrfArg asSetSubstitutionsArg0 = { "substitutions",ioccrfArgString,0};
ioccrfArg *asSetSubstitutionsArgs[1] = {&asSetSubstitutionsArg0};
ioccrfFuncDef asSetSubstitutionsFuncDef =
    {"asSetSubstitutions",1,asSetSubstitutionsArgs};
void asSetSubstitutionsCallFunc(ioccrfArg **args)
{
    asSetSubstitutions((char *)args[0]->value);
}

/* asInit */
ioccrfFuncDef asInitFuncDef = {"asInit",0,0};
void asInitCallFunc(ioccrfArg **args)
{
    asInit();
}

/* asdbdump */
ioccrfFuncDef asdbdumpFuncDef = {"asdbdump",0,0};
void asdbdumpCallFunc(ioccrfArg **args)
{
    asdbdump();
}

/* aspuag */
ioccrfArg aspuagArg0 = { "uagname",ioccrfArgString,0};
ioccrfArg *aspuagArgs[1] = {&aspuagArg0};
ioccrfFuncDef aspuagFuncDef = {"aspuag",1,aspuagArgs};
void aspuagCallFunc(ioccrfArg **args)
{
    aspuag((char *)args[0]->value);
}

/* asphag */
ioccrfArg asphagArg0 = { "hagname",ioccrfArgString,0};
ioccrfArg *asphagArgs[1] = {&asphagArg0};
ioccrfFuncDef asphagFuncDef = {"asphag",1,asphagArgs};
void asphagCallFunc(ioccrfArg **args)
{
    asphag((char *)args[0]->value);
}

/* asprules */
ioccrfArg asprulesArg0 = { "asgname",ioccrfArgString,0};
ioccrfArg *asprulesArgs[1] = {&asprulesArg0};
ioccrfFuncDef asprulesFuncDef = {"asprules",1,asprulesArgs};
void asprulesCallFunc(ioccrfArg **args)
{
    asprules((char *)args[0]->value);
}

/* aspmem */
ioccrfArg aspmemArg0 = { "asgname",ioccrfArgString,0};
ioccrfArg aspmemArg1 = { "asgname",ioccrfArgInt,0};
ioccrfArg *aspmemArgs[2] = {&aspmemArg0,&aspmemArg1};
ioccrfFuncDef aspmemFuncDef = {"aspmem",2,aspmemArgs};
void aspmemCallFunc(ioccrfArg **args)
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
