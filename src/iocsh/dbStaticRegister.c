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

#include "dbAccess.h"
#include "dbStaticLib.h"
#define epicsExportSharedSymbols
#include "registryRecordType.h"
#include "ioccrf.h"
#include "dbStaticRegister.h"

/* dbDumpRecDes */
static const ioccrfArg dbDumpRecDesArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpRecDesArg1 = { "recordTypeName",ioccrfArgString};
static const ioccrfArg *dbDumpRecDesArgs[2] = {&dbDumpRecDesArg0,&dbDumpRecDesArg1};
static const ioccrfFuncDef dbDumpRecDesFuncDef = {"dbDumpRecDes",2,dbDumpRecDesArgs};
static void dbDumpRecDesCallFunc(const ioccrfArgBuf *args)
{
    dbDumpRecDes(pdbbase,args[1].sval);
}

/* dbDumpPath */
static const ioccrfArg dbDumpPathArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg *dbDumpPathArgs[1] = {&dbDumpPathArg0};
static const ioccrfFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
static void dbDumpPathCallFunc(const ioccrfArgBuf *args)
{
    dbDumpPath(pdbbase);
}

/* dbDumpRecord */
static const ioccrfArg dbDumpRecordArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpRecordArg1 = { "recordTypeName",ioccrfArgString};
static const ioccrfArg dbDumpRecordArg2 = { "interest_level",ioccrfArgInt};
static const ioccrfArg *dbDumpRecordArgs[3] =
    {&dbDumpRecordArg0,&dbDumpRecordArg1,&dbDumpRecordArg2};
static const ioccrfFuncDef dbDumpRecordFuncDef = {"dbDumpRecord",3,dbDumpRecordArgs};
static void dbDumpRecordCallFunc(const ioccrfArgBuf *args)
{
    dbDumpRecord(pdbbase,args[1].sval,args[2].ival);
}

/* dbDumpMenu */
static const ioccrfArg dbDumpMenuArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpMenuArg1 = { "menuName",ioccrfArgString};
static const ioccrfArg *dbDumpMenuArgs[2] = {&dbDumpMenuArg0,&dbDumpMenuArg1};
static const ioccrfFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
static void dbDumpMenuCallFunc(const ioccrfArgBuf *args)
{
    dbDumpMenu(pdbbase,args[1].sval);
}

/* dbDumpRecordType */
static const ioccrfArg dbDumpRecordTypeArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpRecordTypeArg1 = { "recordTypeName",ioccrfArgString};
static const ioccrfArg *dbDumpRecordTypeArgs[2] =
    {&dbDumpRecordTypeArg0,&dbDumpRecordTypeArg1};
static const ioccrfFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
static void dbDumpRecordTypeCallFunc(const ioccrfArgBuf *args)
{
    dbDumpRecordType(pdbbase,args[1].sval);
}

/* dbDumpFldDes */
static const ioccrfArg dbDumpFldDesArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpFldDesArg1 = { "recordTypeName",ioccrfArgString};
static const ioccrfArg dbDumpFldDesArg2 = { "fieldName",ioccrfArgString};
static const ioccrfArg *dbDumpFldDesArgs[3] =
    {&dbDumpFldDesArg0,&dbDumpFldDesArg1,&dbDumpFldDesArg2};
static const ioccrfFuncDef dbDumpFldDesFuncDef = {"dbDumpFldDes",3,dbDumpFldDesArgs};
static void dbDumpFldDesCallFunc(const ioccrfArgBuf *args)
{
    dbDumpFldDes(pdbbase,args[1].sval,args[2].sval);
}

/* dbDumpDevice */
static const ioccrfArg dbDumpDeviceArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpDeviceArg1 = { "recordTypeName",ioccrfArgString};
static const ioccrfArg *dbDumpDeviceArgs[2] = {&dbDumpDeviceArg0,&dbDumpDeviceArg1};
static const ioccrfFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
static void dbDumpDeviceCallFunc(const ioccrfArgBuf *args)
{
    dbDumpDevice(pdbbase,args[1].sval);
}

/* dbDumpDriver */
static const ioccrfArg dbDumpDriverArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg *dbDumpDriverArgs[1] = {&dbDumpDriverArg0};
static const ioccrfFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
static void dbDumpDriverCallFunc(const ioccrfArgBuf *args)
{
    dbDumpDriver(pdbbase);
}

/* dbDumpBreaktable */
static const ioccrfArg dbDumpBreaktableArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbDumpBreaktableArg1 = { "tableName",ioccrfArgString};
static const ioccrfArg *dbDumpBreaktableArgs[2] =
    {&dbDumpBreaktableArg0,&dbDumpBreaktableArg1};
static const ioccrfFuncDef dbDumpBreaktableFuncDef =
    {"dbDumpBreaktable",2,dbDumpBreaktableArgs};
static void dbDumpBreaktableCallFunc(const ioccrfArgBuf *args)
{
    dbDumpBreaktable(pdbbase,args[1].sval);
}

/* dbPvdDump */
static const ioccrfArg dbPvdDumpArg0 = { "pdbbase",ioccrfArgPdbbase};
static const ioccrfArg dbPvdDumpArg1 = { "verbose",ioccrfArgInt};
static const ioccrfArg *dbPvdDumpArgs[2] = {&dbPvdDumpArg0,&dbPvdDumpArg1};
static const ioccrfFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
static void dbPvdDumpCallFunc(const ioccrfArgBuf *args)
{
    dbPvdDump(pdbbase,args[1].ival);
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
