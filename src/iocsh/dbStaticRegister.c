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
#include "iocsh.h"
#include "dbStaticRegister.h"

/* dbDumpRecDes */
static const iocshArg dbDumpRecDesArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpRecDesArg1 = { "recordTypeName",iocshArgString};
static const iocshArg * const dbDumpRecDesArgs[2] = {&dbDumpRecDesArg0,&dbDumpRecDesArg1};
static const iocshFuncDef dbDumpRecDesFuncDef = {"dbDumpRecDes",2,dbDumpRecDesArgs};
static void dbDumpRecDesCallFunc(const iocshArgBuf *args)
{
    dbDumpRecDes(pdbbase,args[1].sval);
}

/* dbDumpPath */
static const iocshArg dbDumpPathArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpPathArgs[1] = {&dbDumpPathArg0};
static const iocshFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
static void dbDumpPathCallFunc(const iocshArgBuf *args)
{
    dbDumpPath(pdbbase);
}

/* dbDumpRecord */
static const iocshArg dbDumpRecordArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpRecordArg1 = { "recordTypeName",iocshArgString};
static const iocshArg dbDumpRecordArg2 = { "interest level",iocshArgInt};
static const iocshArg * const dbDumpRecordArgs[3] =
    {&dbDumpRecordArg0,&dbDumpRecordArg1,&dbDumpRecordArg2};
static const iocshFuncDef dbDumpRecordFuncDef = {"dbDumpRecord",3,dbDumpRecordArgs};
static void dbDumpRecordCallFunc(const iocshArgBuf *args)
{
    dbDumpRecord(pdbbase,args[1].sval,args[2].ival);
}

/* dbDumpMenu */
static const iocshArg dbDumpMenuArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpMenuArg1 = { "menuName",iocshArgString};
static const iocshArg * const dbDumpMenuArgs[2] = {&dbDumpMenuArg0,&dbDumpMenuArg1};
static const iocshFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
static void dbDumpMenuCallFunc(const iocshArgBuf *args)
{
    dbDumpMenu(pdbbase,args[1].sval);
}

/* dbDumpRecordType */
static const iocshArg dbDumpRecordTypeArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpRecordTypeArg1 = { "recordTypeName",iocshArgString};
static const iocshArg * const dbDumpRecordTypeArgs[2] =
    {&dbDumpRecordTypeArg0,&dbDumpRecordTypeArg1};
static const iocshFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
static void dbDumpRecordTypeCallFunc(const iocshArgBuf *args)
{
    dbDumpRecordType(pdbbase,args[1].sval);
}

/* dbDumpFldDes */
static const iocshArg dbDumpFldDesArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpFldDesArg1 = { "recordTypeName",iocshArgString};
static const iocshArg dbDumpFldDesArg2 = { "fieldName",iocshArgString};
static const iocshArg * const dbDumpFldDesArgs[3] =
    {&dbDumpFldDesArg0,&dbDumpFldDesArg1,&dbDumpFldDesArg2};
static const iocshFuncDef dbDumpFldDesFuncDef = {"dbDumpFldDes",3,dbDumpFldDesArgs};
static void dbDumpFldDesCallFunc(const iocshArgBuf *args)
{
    dbDumpFldDes(pdbbase,args[1].sval,args[2].sval);
}

/* dbDumpDevice */
static const iocshArg dbDumpDeviceArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpDeviceArg1 = { "recordTypeName",iocshArgString};
static const iocshArg * const dbDumpDeviceArgs[2] = {&dbDumpDeviceArg0,&dbDumpDeviceArg1};
static const iocshFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
static void dbDumpDeviceCallFunc(const iocshArgBuf *args)
{
    dbDumpDevice(pdbbase,args[1].sval);
}

/* dbDumpDriver */
static const iocshArg dbDumpDriverArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpDriverArgs[1] = {&dbDumpDriverArg0};
static const iocshFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
static void dbDumpDriverCallFunc(const iocshArgBuf *args)
{
    dbDumpDriver(pdbbase);
}

/* dbDumpBreaktable */
static const iocshArg dbDumpBreaktableArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpBreaktableArg1 = { "tableName",iocshArgString};
static const iocshArg * const dbDumpBreaktableArgs[2] =
    {&dbDumpBreaktableArg0,&dbDumpBreaktableArg1};
static const iocshFuncDef dbDumpBreaktableFuncDef =
    {"dbDumpBreaktable",2,dbDumpBreaktableArgs};
static void dbDumpBreaktableCallFunc(const iocshArgBuf *args)
{
    dbDumpBreaktable(pdbbase,args[1].sval);
}

/* dbPvdDump */
static const iocshArg dbPvdDumpArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbPvdDumpArg1 = { "verbose",iocshArgInt};
static const iocshArg * const dbPvdDumpArgs[2] = {&dbPvdDumpArg0,&dbPvdDumpArg1};
static const iocshFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
static void dbPvdDumpCallFunc(const iocshArgBuf *args)
{
    dbPvdDump(pdbbase,args[1].ival);
}

void epicsShareAPI dbStaticRegister(void)
{
    iocshRegister(&dbDumpRecDesFuncDef,dbDumpRecDesCallFunc);
    iocshRegister(&dbDumpPathFuncDef,dbDumpPathCallFunc);
    iocshRegister(&dbDumpRecordFuncDef,dbDumpRecordCallFunc);
    iocshRegister(&dbDumpMenuFuncDef,dbDumpMenuCallFunc);
    iocshRegister(&dbDumpRecordTypeFuncDef,dbDumpRecordTypeCallFunc);
    iocshRegister(&dbDumpFldDesFuncDef,dbDumpFldDesCallFunc);
    iocshRegister(&dbDumpDeviceFuncDef,dbDumpDeviceCallFunc);
    iocshRegister(&dbDumpDriverFuncDef,dbDumpDriverCallFunc);
    iocshRegister(&dbDumpBreaktableFuncDef,dbDumpBreaktableCallFunc);
    iocshRegister(&dbPvdDumpFuncDef,dbPvdDumpCallFunc);
}
