/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "iocsh.h"

#define epicsExportSharedSymbols
#include "dbStaticIocRegister.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"

/* common arguments */

static const iocshArg argPdbbase = { "pdbbase", iocshArgPdbbase};
static const iocshArg argRecType = { "recordTypeName", iocshArgString};


/* dbDumpPath */
static const iocshArg * const dbDumpPathArgs[] = {&argPdbbase};
static const iocshFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
static void dbDumpPathCallFunc(const iocshArgBuf *args)
{
    dbDumpPath(*iocshPpdbbase);
}

/* dbDumpRecord */
static const iocshArg dbDumpRecordArg2 = { "interest level",iocshArgInt};
static const iocshArg * const dbDumpRecordArgs[] =
    {&argPdbbase, &argRecType, &dbDumpRecordArg2};
static const iocshFuncDef dbDumpRecordFuncDef =
    {"dbDumpRecord",3,dbDumpRecordArgs};
static void dbDumpRecordCallFunc(const iocshArgBuf *args)
{
    dbDumpRecord(*iocshPpdbbase,args[1].sval,args[2].ival);
}

/* dbDumpMenu */
static const iocshArg dbDumpMenuArg1 = { "menuName",iocshArgString};
static const iocshArg * const dbDumpMenuArgs[] = {
    &argPdbbase, &dbDumpMenuArg1};
static const iocshFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
static void dbDumpMenuCallFunc(const iocshArgBuf *args)
{
    dbDumpMenu(*iocshPpdbbase,args[1].sval);
}

/* dbDumpRecordType */
static const iocshArg * const dbDumpRecordTypeArgs[] =
    {&argPdbbase, &argRecType};
static const iocshFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
static void dbDumpRecordTypeCallFunc(const iocshArgBuf *args)
{
    dbDumpRecordType(*iocshPpdbbase,args[1].sval);
}

/* dbDumpField */
static const iocshArg dbDumpFieldArg2 = { "fieldName",iocshArgString};
static const iocshArg * const dbDumpFieldArgs[] =
    {&argPdbbase, &argRecType,&dbDumpFieldArg2};
static const iocshFuncDef dbDumpFieldFuncDef = {"dbDumpField",3,dbDumpFieldArgs};
static void dbDumpFieldCallFunc(const iocshArgBuf *args)
{
    dbDumpField(*iocshPpdbbase,args[1].sval,args[2].sval);
}

/* dbDumpDevice */
static const iocshArg * const dbDumpDeviceArgs[] = {
    &argPdbbase, &argRecType};
static const iocshFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
static void dbDumpDeviceCallFunc(const iocshArgBuf *args)
{
    dbDumpDevice(*iocshPpdbbase,args[1].sval);
}

/* dbDumpDriver */
static const iocshArg * const dbDumpDriverArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
static void dbDumpDriverCallFunc(const iocshArgBuf *args)
{
    dbDumpDriver(*iocshPpdbbase);
}

/* dbDumpLink */
static const iocshArg * const dbDumpLinkArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpLinkFuncDef = {"dbDumpLink",1,dbDumpLinkArgs};
static void dbDumpLinkCallFunc(const iocshArgBuf *args)
{
    dbDumpLink(*iocshPpdbbase);
}

/* dbDumpRegistrar */
static const iocshArg * const dbDumpRegistrarArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpRegistrarFuncDef = {"dbDumpRegistrar",1,dbDumpRegistrarArgs};
static void dbDumpRegistrarCallFunc(const iocshArgBuf *args)
{
    dbDumpRegistrar(*iocshPpdbbase);
}

/* dbDumpFunction */
static const iocshArg * const dbDumpFunctionArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpFunctionFuncDef = {"dbDumpFunction",1,dbDumpFunctionArgs};
static void dbDumpFunctionCallFunc(const iocshArgBuf *args)
{
    dbDumpFunction(*iocshPpdbbase);
}

/* dbDumpVariable */
static const iocshArg * const dbDumpVariableArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpVariableFuncDef = {"dbDumpVariable",1,dbDumpVariableArgs};
static void dbDumpVariableCallFunc(const iocshArgBuf *args)
{
    dbDumpVariable(*iocshPpdbbase);
}

/* dbDumpBreaktable */
static const iocshArg dbDumpBreaktableArg1 = { "tableName",iocshArgString};
static const iocshArg * const dbDumpBreaktableArgs[] =
    {&argPdbbase,&dbDumpBreaktableArg1};
static const iocshFuncDef dbDumpBreaktableFuncDef =
    {"dbDumpBreaktable",2,dbDumpBreaktableArgs};
static void dbDumpBreaktableCallFunc(const iocshArgBuf *args)
{
    dbDumpBreaktable(*iocshPpdbbase,args[1].sval);
}

/* dbPvdDump */
static const iocshArg dbPvdDumpArg1 = { "verbose",iocshArgInt};
static const iocshArg * const dbPvdDumpArgs[] = {
    &argPdbbase,&dbPvdDumpArg1};
static const iocshFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
static void dbPvdDumpCallFunc(const iocshArgBuf *args)
{
    dbPvdDump(*iocshPpdbbase,args[1].ival);
}

/* dbPvdTableSize */
static const iocshArg dbPvdTableSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const dbPvdTableSizeArgs[1] =
    {&dbPvdTableSizeArg0};
static const iocshFuncDef dbPvdTableSizeFuncDef =
    {"dbPvdTableSize",1,dbPvdTableSizeArgs};
static void dbPvdTableSizeCallFunc(const iocshArgBuf *args)
{
    dbPvdTableSize(args[0].ival);
}

/* dbReportDeviceConfig */
static const iocshArg * const dbReportDeviceConfigArgs[] = {&argPdbbase};
static const iocshFuncDef dbReportDeviceConfigFuncDef = {
    "dbReportDeviceConfig",1,dbReportDeviceConfigArgs};
static void dbReportDeviceConfigCallFunc(const iocshArgBuf *args)
{
    dbReportDeviceConfig(*iocshPpdbbase,stdout);
}

void dbStaticIocRegister(void)
{
    iocshRegister(&dbDumpPathFuncDef, dbDumpPathCallFunc);
    iocshRegister(&dbDumpRecordFuncDef, dbDumpRecordCallFunc);
    iocshRegister(&dbDumpMenuFuncDef, dbDumpMenuCallFunc);
    iocshRegister(&dbDumpRecordTypeFuncDef, dbDumpRecordTypeCallFunc);
    iocshRegister(&dbDumpFieldFuncDef, dbDumpFieldCallFunc);
    iocshRegister(&dbDumpDeviceFuncDef, dbDumpDeviceCallFunc);
    iocshRegister(&dbDumpDriverFuncDef, dbDumpDriverCallFunc);
    iocshRegister(&dbDumpLinkFuncDef, dbDumpLinkCallFunc);
    iocshRegister(&dbDumpRegistrarFuncDef,dbDumpRegistrarCallFunc);
    iocshRegister(&dbDumpFunctionFuncDef, dbDumpFunctionCallFunc);
    iocshRegister(&dbDumpVariableFuncDef, dbDumpVariableCallFunc);
    iocshRegister(&dbDumpBreaktableFuncDef, dbDumpBreaktableCallFunc);
    iocshRegister(&dbPvdDumpFuncDef, dbPvdDumpCallFunc);
    iocshRegister(&dbPvdTableSizeFuncDef,dbPvdTableSizeCallFunc);
    iocshRegister(&dbReportDeviceConfigFuncDef, dbReportDeviceConfigCallFunc);
}
