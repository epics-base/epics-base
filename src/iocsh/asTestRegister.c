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
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "asTestRegister.h"

/* asSetFilename */
static const ioccrfArg asSetFilenameArg0 = { "ascf",ioccrfArgString};
static const ioccrfArg *asSetFilenameArgs[1] = {&asSetFilenameArg0};
static const ioccrfFuncDef asSetFilenameFuncDef =
    {"asSetFilename",1,asSetFilenameArgs};
static void asSetFilenameCallFunc(const ioccrfArgBuf *args)
{
    asSetFilename(args[0].sval);
}

/* asSetSubstitutions */
static const ioccrfArg asSetSubstitutionsArg0 = { "substitutions",ioccrfArgString};
static const ioccrfArg *asSetSubstitutionsArgs[1] = {&asSetSubstitutionsArg0};
static const ioccrfFuncDef asSetSubstitutionsFuncDef =
    {"asSetSubstitutions",1,asSetSubstitutionsArgs};
static void asSetSubstitutionsCallFunc(const ioccrfArgBuf *args)
{
    asSetSubstitutions(args[0].sval);
}

/* asInit */
static const ioccrfFuncDef asInitFuncDef = {"asInit",0};
static void asInitCallFunc(const ioccrfArgBuf *args)
{
    asInit();
}

/* asdbdump */
static const ioccrfFuncDef asdbdumpFuncDef = {"asdbdump",0};
static void asdbdumpCallFunc(const ioccrfArgBuf *args)
{
    asdbdump();
}

/* aspuag */
static const ioccrfArg aspuagArg0 = { "uagname",ioccrfArgString};
static const ioccrfArg *aspuagArgs[1] = {&aspuagArg0};
static const ioccrfFuncDef aspuagFuncDef = {"aspuag",1,aspuagArgs};
static void aspuagCallFunc(const ioccrfArgBuf *args)
{
    aspuag(args[0].sval);
}

/* asphag */
static const ioccrfArg asphagArg0 = { "hagname",ioccrfArgString};
static const ioccrfArg *asphagArgs[1] = {&asphagArg0};
static const ioccrfFuncDef asphagFuncDef = {"asphag",1,asphagArgs};
static void asphagCallFunc(const ioccrfArgBuf *args)
{
    asphag(args[0].sval);
}

/* asprules */
static const ioccrfArg asprulesArg0 = { "asgname",ioccrfArgString};
static const ioccrfArg *asprulesArgs[1] = {&asprulesArg0};
static const ioccrfFuncDef asprulesFuncDef = {"asprules",1,asprulesArgs};
static void asprulesCallFunc(const ioccrfArgBuf *args)
{
    asprules(args[0].sval);
}

/* aspmem */
static const ioccrfArg aspmemArg0 = { "asgname",ioccrfArgString};
static const ioccrfArg aspmemArg1 = { "clients",ioccrfArgInt};
static const ioccrfArg *aspmemArgs[2] = {&aspmemArg0,&aspmemArg1};
static const ioccrfFuncDef aspmemFuncDef = {"aspmem",2,aspmemArgs};
static void aspmemCallFunc(const ioccrfArgBuf *args)
{
    aspmem(args[0].sval,args[1].ival);
}

/* astac */
static const ioccrfArg astacArg0 = { "recordname",ioccrfArgString};
static const ioccrfArg astacArg1 = { "user",ioccrfArgString};
static const ioccrfArg astacArg2 = { "location",ioccrfArgString};
static const ioccrfArg *astacArgs[3] = {&astacArg0,&astacArg1,&astacArg2};
static const ioccrfFuncDef astacFuncDef = {"astac",3,astacArgs};
static void astacCallFunc(const ioccrfArgBuf *args)
{
    astac(args[0].sval,args[1].sval,args[2].sval);
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
    ioccrfRegister(&astacFuncDef,astacCallFunc);
}
