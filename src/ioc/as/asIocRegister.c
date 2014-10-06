/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "asLib.h"
#include "iocsh.h"

#define epicsExportSharedSymbols
#include "asCa.h"
#include "asDbLib.h"
#include "asIocRegister.h"

/* asSetFilename */
static const iocshArg asSetFilenameArg0 = { "ascf",iocshArgString};
static const iocshArg * const asSetFilenameArgs[] = {&asSetFilenameArg0};
static const iocshFuncDef asSetFilenameFuncDef =
    {"asSetFilename",1,asSetFilenameArgs};
static void asSetFilenameCallFunc(const iocshArgBuf *args)
{
    asSetFilename(args[0].sval);
}

/* asSetSubstitutions */
static const iocshArg asSetSubstitutionsArg0 = { "substitutions",iocshArgString};
static const iocshArg * const asSetSubstitutionsArgs[] = {&asSetSubstitutionsArg0};
static const iocshFuncDef asSetSubstitutionsFuncDef =
    {"asSetSubstitutions",1,asSetSubstitutionsArgs};
static void asSetSubstitutionsCallFunc(const iocshArgBuf *args)
{
    asSetSubstitutions(args[0].sval);
}

/* asInit */
static const iocshFuncDef asInitFuncDef = {"asInit",0};
static void asInitCallFunc(const iocshArgBuf *args)
{
    asInit();
}

/* asdbdump */
static const iocshFuncDef asdbdumpFuncDef = {"asdbdump",0};
static void asdbdumpCallFunc(const iocshArgBuf *args)
{
    asdbdump();
}

/* aspuag */
static const iocshArg aspuagArg0 = { "uagname",iocshArgString};
static const iocshArg * const aspuagArgs[] = {&aspuagArg0};
static const iocshFuncDef aspuagFuncDef = {"aspuag",1,aspuagArgs};
static void aspuagCallFunc(const iocshArgBuf *args)
{
    aspuag(args[0].sval);
}

/* asphag */
static const iocshArg asphagArg0 = { "hagname",iocshArgString};
static const iocshArg * const asphagArgs[] = {&asphagArg0};
static const iocshFuncDef asphagFuncDef = {"asphag",1,asphagArgs};
static void asphagCallFunc(const iocshArgBuf *args)
{
    asphag(args[0].sval);
}

/* asprules */
static const iocshArg asprulesArg0 = { "asgname",iocshArgString};
static const iocshArg * const asprulesArgs[] = {&asprulesArg0};
static const iocshFuncDef asprulesFuncDef = {"asprules",1,asprulesArgs};
static void asprulesCallFunc(const iocshArgBuf *args)
{
    asprules(args[0].sval);
}

/* aspmem */
static const iocshArg aspmemArg0 = { "asgname",iocshArgString};
static const iocshArg aspmemArg1 = { "clients",iocshArgInt};
static const iocshArg * const aspmemArgs[] = {&aspmemArg0,&aspmemArg1};
static const iocshFuncDef aspmemFuncDef = {"aspmem",2,aspmemArgs};
static void aspmemCallFunc(const iocshArgBuf *args)
{
    aspmem(args[0].sval,args[1].ival);
}

/* astac */
static const iocshArg astacArg0 = { "recordname",iocshArgString};
static const iocshArg astacArg1 = { "user",iocshArgString};
static const iocshArg astacArg2 = { "location",iocshArgString};
static const iocshArg * const astacArgs[] = {&astacArg0,&astacArg1,&astacArg2};
static const iocshFuncDef astacFuncDef = {"astac",3,astacArgs};
static void astacCallFunc(const iocshArgBuf *args)
{
    astac(args[0].sval,args[1].sval,args[2].sval);
}

/* ascar */
static const iocshArg ascarArg0 = { "level",iocshArgInt};
static const iocshArg * const ascarArgs[] = {&ascarArg0};
static const iocshFuncDef ascarFuncDef = {"ascar",1,ascarArgs};
static void ascarCallFunc(const iocshArgBuf *args)
{
    ascar(args[0].ival);
}

/* asDumpHash */
static const iocshFuncDef asDumpHashFuncDef = {"asDumpHash",0,0};
static void asDumpHashCallFunc(const iocshArgBuf *args)
{
    asDumpHash();
}

void asIocRegister(void)
{
    iocshRegister(&asSetFilenameFuncDef,asSetFilenameCallFunc);
    iocshRegister(&asSetSubstitutionsFuncDef,asSetSubstitutionsCallFunc);
    iocshRegister(&asInitFuncDef,asInitCallFunc);
    iocshRegister(&asdbdumpFuncDef,asdbdumpCallFunc);
    iocshRegister(&aspuagFuncDef,aspuagCallFunc);
    iocshRegister(&asphagFuncDef,asphagCallFunc);
    iocshRegister(&asprulesFuncDef,asprulesCallFunc);
    iocshRegister(&aspmemFuncDef,aspmemCallFunc);
    iocshRegister(&astacFuncDef,astacCallFunc);
    iocshRegister(&ascarFuncDef,ascarCallFunc);
    iocshRegister(&asDumpHashFuncDef,asDumpHashCallFunc);
}
