/* dbStaticRegister.c */
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
#include "dbAccess.h"
#include "dbStaticLib.h"
#include "registryRecordType.h"
#define epicsExportSharedSymbols
#include "dbStaticRegister.h"

/* dbDumpRecDes */
static ioccrfArg dbDumpRecDesArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpRecDesArg1 = { "recordTypeName",ioccrfArgString,0};
static ioccrfArg *dbDumpRecDesArgs[2] = {&dbDumpRecDesArg0,&dbDumpRecDesArg1};
static ioccrfFuncDef dbDumpRecDesFuncDef = {"dbDumpRecDes",2,dbDumpRecDesArgs};
static void dbDumpRecDesCallFunc(ioccrfArg **args)
{
    dbDumpRecDes(pdbbase,(char *)args[1]->value);
}

/* dbDumpPath */
static ioccrfArg dbDumpPathArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg *dbDumpPathArgs[1] = {&dbDumpPathArg0};
static ioccrfFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
static void dbDumpPathCallFunc(ioccrfArg **args)
{
    dbDumpPath(pdbbase);
}

/* dbDumpRecord */
static ioccrfArg dbDumpRecordArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpRecordArg1 = { "recordTypeName",ioccrfArgString,0};
static ioccrfArg dbDumpRecordArg2 = { "interest_level",ioccrfArgInt,0};
static ioccrfArg *dbDumpRecordArgs[3] =
    {&dbDumpRecordArg0,&dbDumpRecordArg1,&dbDumpRecordArg2};
static ioccrfFuncDef dbDumpRecordFuncDef = {"dbDumpRecord",3,dbDumpRecordArgs};
static void dbDumpRecordCallFunc(ioccrfArg **args)
{
    dbDumpRecord(pdbbase,(char *)args[1]->value,*(int *)args[2]->value);
}

/* dbDumpMenu */
static ioccrfArg dbDumpMenuArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpMenuArg1 = { "menuName",ioccrfArgString,0};
static ioccrfArg *dbDumpMenuArgs[2] = {&dbDumpMenuArg0,&dbDumpMenuArg1};
static ioccrfFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
static void dbDumpMenuCallFunc(ioccrfArg **args)
{
    dbDumpMenu(pdbbase,(char *)args[1]->value);
}

/* dbDumpRecordType */
static ioccrfArg dbDumpRecordTypeArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpRecordTypeArg1 = { "recordTypeName",ioccrfArgString,0};
static ioccrfArg *dbDumpRecordTypeArgs[2] =
    {&dbDumpRecordTypeArg0,&dbDumpRecordTypeArg1};
static ioccrfFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
static void dbDumpRecordTypeCallFunc(ioccrfArg **args)
{
    dbDumpRecordType(pdbbase,(char *)args[1]->value);
}

/* dbDumpFldDes */
static ioccrfArg dbDumpFldDesArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpFldDesArg1 = { "recordTypeName",ioccrfArgString,0};
static ioccrfArg dbDumpFldDesArg2 = { "fieldName",ioccrfArgString,0};
static ioccrfArg *dbDumpFldDesArgs[3] =
    {&dbDumpFldDesArg0,&dbDumpFldDesArg1,&dbDumpFldDesArg2};
static ioccrfFuncDef dbDumpFldDesFuncDef = {"dbDumpFldDes",3,dbDumpFldDesArgs};
static void dbDumpFldDesCallFunc(ioccrfArg **args)
{
    dbDumpFldDes(pdbbase,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbDumpDevice */
static ioccrfArg dbDumpDeviceArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpDeviceArg1 = { "recordTypeName",ioccrfArgString,0};
static ioccrfArg *dbDumpDeviceArgs[2] = {&dbDumpDeviceArg0,&dbDumpDeviceArg1};
static ioccrfFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
static void dbDumpDeviceCallFunc(ioccrfArg **args)
{
    dbDumpDevice(pdbbase,(char *)args[1]->value);
}

/* dbDumpDriver */
static ioccrfArg dbDumpDriverArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg *dbDumpDriverArgs[1] = {&dbDumpDriverArg0};
static ioccrfFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
static void dbDumpDriverCallFunc(ioccrfArg **args)
{
    dbDumpDriver(pdbbase);
}

/* dbDumpBreaktable */
static ioccrfArg dbDumpBreaktableArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbDumpBreaktableArg1 = { "tableName",ioccrfArgString,0};
static ioccrfArg *dbDumpBreaktableArgs[2] =
    {&dbDumpBreaktableArg0,&dbDumpBreaktableArg1};
static ioccrfFuncDef dbDumpBreaktableFuncDef =
    {"dbDumpBreaktable",2,dbDumpBreaktableArgs};
static void dbDumpBreaktableCallFunc(ioccrfArg **args)
{
    dbDumpBreaktable(pdbbase,(char *)args[1]->value);
}

/* dbPvdDump */
static ioccrfArg dbPvdDumpArg0 = { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg dbPvdDumpArg1 = { "verbose",ioccrfArgInt,0};
static ioccrfArg *dbPvdDumpArgs[2] = {&dbPvdDumpArg0,&dbPvdDumpArg1};
static ioccrfFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
static void dbPvdDumpCallFunc(ioccrfArg **args)
{
    dbPvdDump(pdbbase,*(int *)args[1]->value);
}

void epicsShareAPI dbStaticRegister(void)
{
    ioccrfRegister(&dbDumpRecDesFuncDef,dbDumpRecDesCallFunc);
    ioccrfRegister(&dbDumpPathFuncDef,dbDumpPathCallFunc);
    ioccrfRegister(&dbDumpRecordFuncDef,dbDumpRecordCallFunc);
    ioccrfRegister(&dbDumpMenuFuncDef,dbDumpMenuCallFunc);
    ioccrfRegister(&dbDumpRecordTypeFuncDef,dbDumpRecordTypeCallFunc);
    ioccrfRegister(&dbDumpFldDesFuncDef,dbDumpFldDesCallFunc);
    ioccrfRegister(&dbDumpDeviceFuncDef,dbDumpDeviceCallFunc);
    ioccrfRegister(&dbDumpDriverFuncDef,dbDumpDriverCallFunc);
    ioccrfRegister(&dbDumpBreaktableFuncDef,dbDumpBreaktableCallFunc);
    ioccrfRegister(&dbPvdDumpFuncDef,dbPvdDumpCallFunc);
}
