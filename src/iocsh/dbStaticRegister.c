/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbStaticRegister.c */
/* Author:  Marty Kraimer Date: 02MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "dbAccess.h"
#include "dbStaticLib.h"
#include "epicsStdio.h"
#define epicsExportSharedSymbols
#include "registryRecordType.h"
#include "iocsh.h"
#include "dbStaticRegister.h"

/* dbDumpPath */
static const iocshArg dbDumpPathArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpPathArgs[] = {&dbDumpPathArg0};
static const iocshFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs};
static void dbDumpPathCallFunc(const iocshArgBuf *args)
{
    dbDumpPath(pdbbase);
}

/* dbDumpRecord */
static const iocshArg dbDumpRecordArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpRecordArg1 = { "recordTypeName",iocshArgString};
static const iocshArg dbDumpRecordArg2 = { "interest level",iocshArgInt};
static const iocshArg * const dbDumpRecordArgs[] =
    {&dbDumpRecordArg0,&dbDumpRecordArg1,&dbDumpRecordArg2};
static const iocshFuncDef dbDumpRecordFuncDef =
    {"dbDumpRecord",3,dbDumpRecordArgs};
static void dbDumpRecordCallFunc(const iocshArgBuf *args)
{
    dbDumpRecord(pdbbase,args[1].sval,args[2].ival);
}

/* dbDumpMenu */
static const iocshArg dbDumpMenuArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpMenuArg1 = { "menuName",iocshArgString};
static const iocshArg * const dbDumpMenuArgs[] = {
    &dbDumpMenuArg0,&dbDumpMenuArg1};
static const iocshFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs};
static void dbDumpMenuCallFunc(const iocshArgBuf *args)
{
    dbDumpMenu(pdbbase,args[1].sval);
}

/* dbDumpRecordType */
static const iocshArg dbDumpRecordTypeArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpRecordTypeArg1 = { "recordTypeName",iocshArgString};
static const iocshArg * const dbDumpRecordTypeArgs[] =
    {&dbDumpRecordTypeArg0,&dbDumpRecordTypeArg1};
static const iocshFuncDef dbDumpRecordTypeFuncDef =
    {"dbDumpRecordType",2,dbDumpRecordTypeArgs};
static void dbDumpRecordTypeCallFunc(const iocshArgBuf *args)
{
    dbDumpRecordType(pdbbase,args[1].sval);
}

/* dbDumpField */
static const iocshArg dbDumpFieldArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpFieldArg1 = { "recordTypeName",iocshArgString};
static const iocshArg dbDumpFieldArg2 = { "fieldName",iocshArgString};
static const iocshArg * const dbDumpFieldArgs[] =
    {&dbDumpFieldArg0,&dbDumpFieldArg1,&dbDumpFieldArg2};
static const iocshFuncDef dbDumpFieldFuncDef = {"dbDumpField",3,dbDumpFieldArgs};
static void dbDumpFieldCallFunc(const iocshArgBuf *args)
{
    dbDumpField(pdbbase,args[1].sval,args[2].sval);
}

/* dbDumpDevice */
static const iocshArg dbDumpDeviceArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpDeviceArg1 = { "recordTypeName",iocshArgString};
static const iocshArg * const dbDumpDeviceArgs[] = {
    &dbDumpDeviceArg0,&dbDumpDeviceArg1};

static const iocshFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs};
static void dbDumpDeviceCallFunc(const iocshArgBuf *args)
{
    dbDumpDevice(pdbbase,args[1].sval);
}

/* dbDumpDriver */
static const iocshArg dbDumpDriverArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpDriverArgs[] = { &dbDumpDriverArg0};
static const iocshFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs};
static void dbDumpDriverCallFunc(const iocshArgBuf *args)
{
    dbDumpDriver(pdbbase);
}

/* dbDumpRegistrar */
static const iocshArg dbDumpRegistrarArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpRegistrarArgs[] = { &dbDumpRegistrarArg0};
static const iocshFuncDef dbDumpRegistrarFuncDef = {"dbDumpRegistrar",1,dbDumpRegistrarArgs};
static void dbDumpRegistrarCallFunc(const iocshArgBuf *args)
{
    dbDumpRegistrar(pdbbase);
}

/* dbDumpFunction */
static const iocshArg dbDumpFunctionArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpFunctionArgs[] = { &dbDumpFunctionArg0};
static const iocshFuncDef dbDumpFunctionFuncDef = {"dbDumpFunction",1,dbDumpFunctionArgs};
static void dbDumpFunctionCallFunc(const iocshArgBuf *args)
{
    dbDumpFunction(pdbbase);
}

/* dbDumpVariable */
static const iocshArg dbDumpVariableArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbDumpVariableArgs[] = { &dbDumpVariableArg0};
static const iocshFuncDef dbDumpVariableFuncDef = {"dbDumpVariable",1,dbDumpVariableArgs};
static void dbDumpVariableCallFunc(const iocshArgBuf *args)
{
    dbDumpVariable(pdbbase);
}

/* dbDumpBreaktable */
static const iocshArg dbDumpBreaktableArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg dbDumpBreaktableArg1 = { "tableName",iocshArgString};
static const iocshArg * const dbDumpBreaktableArgs[] =
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
static const iocshArg * const dbPvdDumpArgs[] = {
    &dbPvdDumpArg0,&dbPvdDumpArg1};
static const iocshFuncDef dbPvdDumpFuncDef = {"dbPvdDump",2,dbPvdDumpArgs};
static void dbPvdDumpCallFunc(const iocshArgBuf *args)
{
    dbPvdDump(pdbbase,args[1].ival);
}

/* dbReportDeviceConfig */
static const iocshArg dbReportDeviceConfigArg0 = { "pdbbase",iocshArgPdbbase};
static const iocshArg * const dbReportDeviceConfigArgs[] = {
    &dbReportDeviceConfigArg0};
static const iocshFuncDef dbReportDeviceConfigFuncDef = {
    "dbReportDeviceConfig",1,dbReportDeviceConfigArgs};
static void dbReportDeviceConfigCallFunc(const iocshArgBuf *args)
{
    dbReportDeviceConfig(pdbbase,stdout);
}

void epicsShareAPI dbStaticRegister(void)
{
    iocshRegister(&dbDumpPathFuncDef,dbDumpPathCallFunc);
    iocshRegister(&dbDumpRecordFuncDef,dbDumpRecordCallFunc);
    iocshRegister(&dbDumpMenuFuncDef,dbDumpMenuCallFunc);
    iocshRegister(&dbDumpRecordTypeFuncDef,dbDumpRecordTypeCallFunc);
    iocshRegister(&dbDumpFieldFuncDef,dbDumpFieldCallFunc);
    iocshRegister(&dbDumpDeviceFuncDef,dbDumpDeviceCallFunc);
    iocshRegister(&dbDumpDriverFuncDef,dbDumpDriverCallFunc);
    iocshRegister(&dbDumpRegistrarFuncDef,dbDumpRegistrarCallFunc);
    iocshRegister(&dbDumpFunctionFuncDef,dbDumpFunctionCallFunc);
    iocshRegister(&dbDumpVariableFuncDef,dbDumpVariableCallFunc);
    iocshRegister(&dbDumpBreaktableFuncDef,dbDumpBreaktableCallFunc);
    iocshRegister(&dbPvdDumpFuncDef,dbPvdDumpCallFunc);
    iocshRegister(&dbReportDeviceConfigFuncDef,dbReportDeviceConfigCallFunc);
}
