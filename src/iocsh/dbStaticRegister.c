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
ioccrfArg dbDumpRecDesArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpRecDesArg1 = { "recordTypeName",ioccrfArgString,0};
ioccrfArg *dbDumpRecDesArgs[2] = {&dbDumpRecDesArg0,&dbDumpRecDesArg1};
ioccrfFuncDef dbDumpRecDesFuncDef = {"dbDumpRecDes",2,dbDumpRecDesArgs};
void dbDumpRecDesCallFunc(ioccrfArg **args)
{
    dbDumpRecDes(pdbbase,(char *)args[1]->value);
}

/* dbDumpPath */
ioccrfArg dbDumpPathArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg *dbDumpPathArgs[1] = {&dbDumpPathArg0};
ioccrfFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
void dbDumpPathCallFunc(ioccrfArg **args)
{
    dbDumpPath(pdbbase);
}

/* dbDumpRecord */
ioccrfArg dbDumpRecordArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpRecordArg1 = { "recordTypeName",ioccrfArgString,0};
ioccrfArg dbDumpRecordArg2 = { "interest_level",ioccrfArgInt,0};
ioccrfArg *dbDumpRecordArgs[3] =
    {&dbDumpRecordArg0,&dbDumpRecordArg1,&dbDumpRecordArg2};
ioccrfFuncDef dbDumpRecordFuncDef = {"dbDumpRecord",3,dbDumpRecordArgs};
void dbDumpRecordCallFunc(ioccrfArg **args)
{
    dbDumpRecord(pdbbase,(char *)args[1]->value,*(int *)args[2]->value);
}

/* dbDumpMenu */
ioccrfArg dbDumpMenuArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpMenuArg1 = { "menuName",ioccrfArgString,0};
ioccrfArg *dbDumpMenuArgs[2] = {&dbDumpMenuArg0,&dbDumpMenuArg1};
ioccrfFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
void dbDumpMenuCallFunc(ioccrfArg **args)
{
    dbDumpMenu(pdbbase,(char *)args[1]->value);
}

/* dbDumpRecordType */
ioccrfArg dbDumpRecordTypeArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpRecordTypeArg1 = { "recordTypeName",ioccrfArgString,0};
ioccrfArg *dbDumpRecordTypeArgs[2] =
    {&dbDumpRecordTypeArg0,&dbDumpRecordTypeArg1};
ioccrfFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
void dbDumpRecordTypeCallFunc(ioccrfArg **args)
{
    dbDumpRecordType(pdbbase,(char *)args[1]->value);
}

/* dbDumpFldDes */
ioccrfArg dbDumpFldDesArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpFldDesArg1 = { "recordTypeName",ioccrfArgString,0};
ioccrfArg dbDumpFldDesArg2 = { "fieldName",ioccrfArgString,0};
ioccrfArg *dbDumpFldDesArgs[3] =
    {&dbDumpFldDesArg0,&dbDumpFldDesArg1,&dbDumpFldDesArg2};
ioccrfFuncDef dbDumpFldDesFuncDef = {"dbDumpFldDes",3,dbDumpFldDesArgs};
void dbDumpFldDesCallFunc(ioccrfArg **args)
{
    dbDumpFldDes(pdbbase,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbDumpDevice */
ioccrfArg dbDumpDeviceArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpDeviceArg1 = { "recordTypeName",ioccrfArgString,0};
ioccrfArg *dbDumpDeviceArgs[2] = {&dbDumpDeviceArg0,&dbDumpDeviceArg1};
ioccrfFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
void dbDumpDeviceCallFunc(ioccrfArg **args)
{
    dbDumpDevice(pdbbase,(char *)args[1]->value);
}

/* dbDumpDriver */
ioccrfArg dbDumpDriverArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg *dbDumpDriverArgs[1] = {&dbDumpDriverArg0};
ioccrfFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
void dbDumpDriverCallFunc(ioccrfArg **args)
{
    dbDumpDriver(pdbbase);
}

/* dbDumpBreaktable */
ioccrfArg dbDumpBreaktableArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbDumpBreaktableArg1 = { "tableName",ioccrfArgString,0};
ioccrfArg *dbDumpBreaktableArgs[2] =
    {&dbDumpBreaktableArg0,&dbDumpBreaktableArg1};
ioccrfFuncDef dbDumpBreaktableFuncDef =
    {"dbDumpBreaktable",2,dbDumpBreaktableArgs};
void dbDumpBreaktableCallFunc(ioccrfArg **args)
{
    dbDumpBreaktable(pdbbase,(char *)args[1]->value);
}

/* dbPvdDump */
ioccrfArg dbPvdDumpArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg dbPvdDumpArg1 = { "verbose",ioccrfArgInt,0};
ioccrfArg *dbPvdDumpArgs[2] = {&dbPvdDumpArg0,&dbPvdDumpArg1};
ioccrfFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
void dbPvdDumpCallFunc(ioccrfArg **args)
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
