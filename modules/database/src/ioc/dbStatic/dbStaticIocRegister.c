/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "iocsh.h"
#include "errSymTbl.h"

#include "dbStaticIocRegister.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"

/* common arguments */

static const iocshArg argPdbbase = { "pdbbase", iocshArgPdbbase};
static const iocshArg argRecType = { "recordTypeName", iocshArgString};


/* dbDumpPath */
static const iocshArg * const dbDumpPathArgs[] = {&argPdbbase};
static const iocshFuncDef dbDumpPathFuncDef = {"dbDumpPath",1,dbDumpPathArgs,
                                               "Dump .db/.dbd file search path.\n"
                                               "Example: dbDumpPath pdbbase\n"};
static void dbDumpPathCallFunc(const iocshArgBuf *args)
{
    dbDumpPath(*iocshPpdbbase);
}

/* dbDumpRecord */
static const iocshArg dbDumpRecordArg2 = { "interest level",iocshArgInt};
static const iocshArg * const dbDumpRecordArgs[] =
    {&argPdbbase, &argRecType, &dbDumpRecordArg2};
static const iocshFuncDef dbDumpRecordFuncDef = {"dbDumpRecord",3,dbDumpRecordArgs,
                          "Dump information about the recordTypeName with 'interest level' details.\n"
                          "Example: dbDumpRecord ai 2\n"
                          "If last argument(s) are missing, dump all recordType information in the database.\n"};
static void dbDumpRecordCallFunc(const iocshArgBuf *args)
{
    dbDumpRecord(*iocshPpdbbase,args[1].sval,args[2].ival);
}

/* dbDumpMenu */
static const iocshArg dbDumpMenuArg1 = { "menuName",iocshArgString};
static const iocshArg * const dbDumpMenuArgs[] = {
    &argPdbbase, &dbDumpMenuArg1};
static const iocshFuncDef dbDumpMenuFuncDef = {"dbDumpMenu",2,dbDumpMenuArgs,
                          "Dump information about the available menuNames and choices defined withing each menuName.\n"
                          "Example: dbDumpMenu pdbbase menuAlarmStat \n"
                          "If last argument(s) are missing, dump all menuNames information in the database.\n"};
static void dbDumpMenuCallFunc(const iocshArgBuf *args)
{
    dbDumpMenu(*iocshPpdbbase,args[1].sval);
}

/* dbDumpRecordType */
static const iocshArg * const dbDumpRecordTypeArgs[] =
    {&argPdbbase, &argRecType};
static const iocshFuncDef dbDumpRecordTypeFuncDef = {"dbDumpRecordType",2,dbDumpRecordTypeArgs,
                          "Dump information about available fields in the recortTypeName sorted by index and name.\n"
                          "Example: dbDumpRecordType pdbbase calcout\n"
                          "If last argument(s) are missing, dump fields information for all records in the database.\n"};
static void dbDumpRecordTypeCallFunc(const iocshArgBuf *args)
{
    dbDumpRecordType(*iocshPpdbbase,args[1].sval);
}

/* dbDumpField */
static const iocshArg dbDumpFieldArg2 = { "fieldName",iocshArgString};
static const iocshArg * const dbDumpFieldArgs[] =
    {&argPdbbase, &argRecType,&dbDumpFieldArg2};
static const iocshFuncDef dbDumpFieldFuncDef = {"dbDumpField",3,dbDumpFieldArgs,
                          "Dump information about the fieldName in the recordTypeName.\n"
                          "Example: dbDumpField  pdbbase calcout A\n"
                          "If last argument(s) are missing, dump information\n" 
                          "about all fieldName in all recordTypeName in the database.\n"};
static void dbDumpFieldCallFunc(const iocshArgBuf *args)
{
    dbDumpField(*iocshPpdbbase,args[1].sval,args[2].sval);
}

/* dbDumpDevice */
static const iocshArg * const dbDumpDeviceArgs[] = {
    &argPdbbase, &argRecType};
static const iocshFuncDef dbDumpDeviceFuncDef = {"dbDumpDevice",2,dbDumpDeviceArgs,
                          "Dump device support information for the recordTypeName.\n"
                          "Example: dbDumpDevice pdbbase ai\n"
                          "If last argument(s) are missing, dump device support\n"
                          "information for all records in the database.\n"};
static void dbDumpDeviceCallFunc(const iocshArgBuf *args)
{
    dbDumpDevice(*iocshPpdbbase,args[1].sval);
}

/* dbDumpDriver */
static const iocshArg * const dbDumpDriverArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpDriverFuncDef = {"dbDumpDriver",1,dbDumpDriverArgs,
                          "Dump device support information.\n"
                          "Example: dbDumpDriver pdbbase\n"
                          "If the last argument(s) are missing, dump all device support information.\n",};
static void dbDumpDriverCallFunc(const iocshArgBuf *args)
{
    dbDumpDriver(*iocshPpdbbase);
}

/* dbDumpLink */
static const iocshArg * const dbDumpLinkArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpLinkFuncDef = {
    "dbDumpLink",
    1,
    dbDumpLinkArgs,
    "Dump list of registered links\n"
    "Example: dbDumpLink pdbbase\n",
};
static void dbDumpLinkCallFunc(const iocshArgBuf *args)
{
    dbDumpLink(*iocshPpdbbase);
}

/* dbDumpRegistrar */
static const iocshArg * const dbDumpRegistrarArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpRegistrarFuncDef = {"dbDumpRegistrar",1,dbDumpRegistrarArgs,
                          "Dump list of registered functions including ones for subroutine records,\n"
                          "and ones that can be invoked from iocsh.\n"
                          "Example: dbDumpRegistrar pdbbase\n"
                          "If last argument(s) are missing, dump all registered functions\n"};
static void dbDumpRegistrarCallFunc(const iocshArgBuf *args)
{
    dbDumpRegistrar(*iocshPpdbbase);
}

/* dbDumpFunction */
static const iocshArg * const dbDumpFunctionArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpFunctionFuncDef = {"dbDumpFunction",1,dbDumpFunctionArgs,
                          "Dump list of registered subroutine functions.\n"
                          "Example: dbDumpFunction pddbase\n"
                          "If last argument(s) are missing, dump all registered subroutine functions\n"};
static void dbDumpFunctionCallFunc(const iocshArgBuf *args)
{
    dbDumpFunction(*iocshPpdbbase);
}

/* dbDumpVariable */
static const iocshArg * const dbDumpVariableArgs[] = { &argPdbbase};
static const iocshFuncDef dbDumpVariableFuncDef = {"dbDumpVariable",1,dbDumpVariableArgs,
                          "Dump list of variables used in the database.\n"
                          "Example: dbDumpVariable pddbase\n"
                          "If last argument(s) are missing, dump all variables.\n"};
static void dbDumpVariableCallFunc(const iocshArgBuf *args)
{
    dbDumpVariable(*iocshPpdbbase);
}

/* dbDumpBreaktable */
static const iocshArg dbDumpBreaktableArg1 = { "tableName",iocshArgString};
static const iocshArg * const dbDumpBreaktableArgs[] =
    {&argPdbbase,&dbDumpBreaktableArg1};
static const iocshFuncDef dbDumpBreaktableFuncDef = {
    "dbDumpBreaktable",
    2,
    dbDumpBreaktableArgs,
    "Dump the given break table\n"
    "Example: dbDumpBreaktable pdbbase typeKdegC\n"
    "If last argument(s) are missing, dump all breakpoint tables.\n",
};
static void dbDumpBreaktableCallFunc(const iocshArgBuf *args)
{
    dbDumpBreaktable(*iocshPpdbbase,args[1].sval);
}

/* dbPvdDump */
static const iocshArg dbPvdDumpArg1 = { "verbose",iocshArgInt};
static const iocshArg * const dbPvdDumpArgs[] = {
    &argPdbbase,&dbPvdDumpArg1};
static const iocshFuncDef dbPvdDumpFuncDef = {
    "dbPvdDump",
    2,
    dbPvdDumpArgs,
    "Dump the various buckets of the process variable directory.\n"
    "If verbose is greater than 0, also print the process variables in each bucket.\n"
    "Example: dbPvdDump pdbbase 1\n"
    "If the last argument(s) are missing, dump all buckets as though verbose is 0.\n",
};
static void dbPvdDumpCallFunc(const iocshArgBuf *args)
{
    dbPvdDump(*iocshPpdbbase,args[1].ival);
}

/* dbPvdTableSize */
static const iocshArg dbPvdTableSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const dbPvdTableSizeArgs[1] =
    {&dbPvdTableSizeArg0};
static const iocshFuncDef dbPvdTableSizeFuncDef = {
    "dbPvdTableSize",
    1,
    dbPvdTableSizeArgs,
    "Change the number of buckets in the process variable directory.\n\n"
    "The process variable directory size should be set before loading the database.\n"
    "The size of the process variable directory can automatically grow.\n"
    "The size must be a power of 2.\n\n"
    "Example: dbPvdTableSize 1024\n",
};
static void dbPvdTableSizeCallFunc(const iocshArgBuf *args)
{
    dbPvdTableSize(args[0].ival);
}

/* dbReportDeviceConfig */
static const iocshArg * const dbReportDeviceConfigArgs[] = {&argPdbbase};
static const iocshFuncDef dbReportDeviceConfigFuncDef = {
    "dbReportDeviceConfig",
    1,
    dbReportDeviceConfigArgs,
    "Print the link type, link value, device type, record name,\n"
    "and linearisation info (if applicable),\n"
    "for every record using a specific device type.\n\n"
    "Example: dbReportDeviceConfig pdbbase\n",
};
static void dbReportDeviceConfigCallFunc(const iocshArgBuf *args)
{
    dbReportDeviceConfig(*iocshPpdbbase,stdout);
}


static const iocshArg dbCreateAliasArg0 = { "record",iocshArgStringRecord};
static const iocshArg dbCreateAliasArg1 = { "alias",iocshArgStringRecord};
static const iocshArg * const dbCreateAliasArgs[] = {&argPdbbase,&dbCreateAliasArg0, &dbCreateAliasArg1};
static const iocshFuncDef dbCreateAliasFuncDef = {
    "dbCreateAlias",
    3,
    dbCreateAliasArgs,
    "Add a new record alias.\n"
    "\n"
    "Example: dbCreateAlias pdbbase record:name new:alias\n",
};
static void dbCreateAliasCallFunc(const iocshArgBuf *args)
{
    DBENTRY ent;
    long status;

    dbInitEntry(*iocshPpdbbase, &ent);
    if(!args[1].sval || !args[2].sval) {
        status = S_dbLib_recNotFound;

    } else {
        status = dbFindRecord(&ent, args[1].sval);
        if(!status) {
            status = dbCreateAlias(&ent, args[2].sval);
        }
    }
    dbFinishEntry(&ent);
    if(status) {
        fprintf(stderr, "Error: %ld %s\n", status, errSymMsg(status));
        iocshSetError(1);
    }
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
    iocshRegister(&dbCreateAliasFuncDef, dbCreateAliasCallFunc);
}
