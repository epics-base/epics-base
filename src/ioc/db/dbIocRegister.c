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
#include "dbBkpt.h"
#include "dbAccess.h"
#include "dbCaTest.h"
#include "dbEvent.h"
#include "dbTest.h"
#include "db_test.h"
#include "dbLock.h"
#include "dbScan.h"
#include "dbNotify.h"
#include "callback.h"
#include "dbIocRegister.h"

/* dbLoadDatabase */
static const iocshArg dbLoadDatabaseArg0 = { "file name",iocshArgString};
static const iocshArg dbLoadDatabaseArg1 = { "path",iocshArgString};
static const iocshArg dbLoadDatabaseArg2 = { "substitutions",iocshArgString};
static const iocshArg * const dbLoadDatabaseArgs[3] =
{
    &dbLoadDatabaseArg0,&dbLoadDatabaseArg1,&dbLoadDatabaseArg2
};
static const iocshFuncDef dbLoadDatabaseFuncDef =
    {"dbLoadDatabase",3,dbLoadDatabaseArgs};
static void dbLoadDatabaseCallFunc(const iocshArgBuf *args)
{
    dbLoadDatabase(args[0].sval,args[1].sval,args[2].sval);
}

/* dbLoadRecords */
static const iocshArg dbLoadRecordsArg0 = { "file name",iocshArgString};
static const iocshArg dbLoadRecordsArg1 = { "substitutions",iocshArgString};
static const iocshArg * const dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
static const iocshFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
static void dbLoadRecordsCallFunc(const iocshArgBuf *args)
{
    dbLoadRecords(args[0].sval,args[1].sval);
}

/* dbb */
static const iocshArg dbbArg0 = { "record name",iocshArgString};
static const iocshArg * const dbbArgs[1] = {&dbbArg0};
static const iocshFuncDef dbbFuncDef = {"dbb",1,dbbArgs};
static void dbbCallFunc(const iocshArgBuf *args) { dbb(args[0].sval);}

/* dbd */
static const iocshArg dbdArg0 = { "record name",iocshArgString};
static const iocshArg * const dbdArgs[1] = {&dbdArg0};
static const iocshFuncDef dbdFuncDef = {"dbd",1,dbdArgs};
static void dbdCallFunc(const iocshArgBuf *args) { dbd(args[0].sval);}

/* dbc */
static const iocshArg dbcArg0 = { "record name",iocshArgString};
static const iocshArg * const dbcArgs[1] = {&dbcArg0};
static const iocshFuncDef dbcFuncDef = {"dbc",1,dbcArgs};
static void dbcCallFunc(const iocshArgBuf *args) { dbc(args[0].sval);}

/* dbs */
static const iocshArg dbsArg0 = { "record name",iocshArgString};
static const iocshArg * const dbsArgs[1] = {&dbsArg0};
static const iocshFuncDef dbsFuncDef = {"dbs",1,dbsArgs};
static void dbsCallFunc(const iocshArgBuf *args) { dbs(args[0].sval);}

/* dbstat */
static const iocshFuncDef dbstatFuncDef = {"dbstat",0};
static void dbstatCallFunc(const iocshArgBuf *args) { dbstat();}

/* dbp */
static const iocshArg dbpArg0 = { "record name",iocshArgString};
static const iocshArg dbpArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbpArgs[2] = {&dbpArg0,&dbpArg1};
static const iocshFuncDef dbpFuncDef = {"dbp",2,dbpArgs};
static void dbpCallFunc(const iocshArgBuf *args)
{ dbp(args[0].sval,args[1].ival);}

/* dbap */
static const iocshArg dbapArg0 = { "record name",iocshArgString};
static const iocshArg * const dbapArgs[1] = {&dbapArg0};
static const iocshFuncDef dbapFuncDef = {"dbap",1,dbapArgs};
static void dbapCallFunc(const iocshArgBuf *args) { dbap(args[0].sval);}

/* dbcar */
static const iocshArg dbcarArg0 = { "record name",iocshArgString};
static const iocshArg dbcarArg1 = { "level",iocshArgInt};
static const iocshArg * const dbcarArgs[2] = {&dbcarArg0,&dbcarArg1};
static const iocshFuncDef dbcarFuncDef = {"dbcar",2,dbcarArgs};
static void dbcarCallFunc(const iocshArgBuf *args)
{
    dbcar(args[0].sval,args[1].ival);
}

/* dbel */
static const iocshArg dbelArg0 = { "record name",iocshArgString};
static const iocshArg dbelArg1 = { "level",iocshArgInt};
static const iocshArg * const dbelArgs[2] = {&dbelArg0,&dbelArg1};
static const iocshFuncDef dbelFuncDef = {"dbel",2,dbelArgs};
static void dbelCallFunc(const iocshArgBuf *args)
{
    dbel(args[0].sval, args[1].ival);
}

/* dba */
static const iocshArg dbaArg0 = { "record name",iocshArgString};
static const iocshArg * const dbaArgs[1] = {&dbaArg0};
static const iocshFuncDef dbaFuncDef = {"dba",1,dbaArgs};
static void dbaCallFunc(const iocshArgBuf *args) { dba(args[0].sval);}

/* dbl */
static const iocshArg dblArg0 = { "record type",iocshArgString};
static const iocshArg dblArg1 = { "fields",iocshArgString};
static const iocshArg * const dblArgs[] = {&dblArg0,&dblArg1};
static const iocshFuncDef dblFuncDef = {"dbl",2,dblArgs};
static void dblCallFunc(const iocshArgBuf *args)
{
    dbl(args[0].sval,args[1].sval);
}

/* dbnr */
static const iocshArg dbnrArg0 = { "verbose",iocshArgInt};
static const iocshArg * const dbnrArgs[1] = {&dbnrArg0};
static const iocshFuncDef dbnrFuncDef = {"dbnr",1,dbnrArgs};
static void dbnrCallFunc(const iocshArgBuf *args) { dbnr(args[0].ival);}

/* dbla */
static const iocshArg dblaArg0 = { "pattern",iocshArgString};
static const iocshArg * const dblaArgs[1] = {&dblaArg0};
static const iocshFuncDef dblaFuncDef = {"dbla",1,dblaArgs};
static void dblaCallFunc(const iocshArgBuf *args) { dbla(args[0].sval);}

/* dbgrep */
static const iocshArg dbgrepArg0 = { "pattern",iocshArgString};
static const iocshArg * const dbgrepArgs[1] = {&dbgrepArg0};
static const iocshFuncDef dbgrepFuncDef = {"dbgrep",1,dbgrepArgs};
static void dbgrepCallFunc(const iocshArgBuf *args) { dbgrep(args[0].sval);}

/* dbgf */
static const iocshArg dbgfArg0 = { "record name",iocshArgString};
static const iocshArg * const dbgfArgs[1] = {&dbgfArg0};
static const iocshFuncDef dbgfFuncDef = {"dbgf",1,dbgfArgs};
static void dbgfCallFunc(const iocshArgBuf *args) { dbgf(args[0].sval);}

/* dbpf */
static const iocshArg dbpfArg0 = { "record name",iocshArgString};
static const iocshArg dbpfArg1 = { "value",iocshArgString};
static const iocshArg * const dbpfArgs[2] = {&dbpfArg0,&dbpfArg1};
static const iocshFuncDef dbpfFuncDef = {"dbpf",2,dbpfArgs};
static void dbpfCallFunc(const iocshArgBuf *args)
{ dbpf(args[0].sval,args[1].sval);}

/* dbpr */
static const iocshArg dbprArg0 = { "record name",iocshArgString};
static const iocshArg dbprArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbprArgs[2] = {&dbprArg0,&dbprArg1};
static const iocshFuncDef dbprFuncDef = {"dbpr",2,dbprArgs};
static void dbprCallFunc(const iocshArgBuf *args)
{ dbpr(args[0].sval,args[1].ival);}

/* dbtr */
static const iocshArg dbtrArg0 = { "record name",iocshArgString};
static const iocshArg * const dbtrArgs[1] = {&dbtrArg0};
static const iocshFuncDef dbtrFuncDef = {"dbtr",1,dbtrArgs};
static void dbtrCallFunc(const iocshArgBuf *args) { dbtr(args[0].sval);}

/* dbtgf */
static const iocshArg dbtgfArg0 = { "record name",iocshArgString};
static const iocshArg * const dbtgfArgs[1] = {&dbtgfArg0};
static const iocshFuncDef dbtgfFuncDef = {"dbtgf",1,dbtgfArgs};
static void dbtgfCallFunc(const iocshArgBuf *args) { dbtgf(args[0].sval);}

/* dbtpf */
static const iocshArg dbtpfArg0 = { "record name",iocshArgString};
static const iocshArg dbtpfArg1 = { "value",iocshArgString};
static const iocshArg * const dbtpfArgs[2] = {&dbtpfArg0,&dbtpfArg1};
static const iocshFuncDef dbtpfFuncDef = {"dbtpf",2,dbtpfArgs};
static void dbtpfCallFunc(const iocshArgBuf *args)
{ dbtpf(args[0].sval,args[1].sval);}

/* dbior */
static const iocshArg dbiorArg0 = { "driver name",iocshArgString};
static const iocshArg dbiorArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbiorArgs[] = {&dbiorArg0,&dbiorArg1};
static const iocshFuncDef dbiorFuncDef = {"dbior",2,dbiorArgs};
static void dbiorCallFunc(const iocshArgBuf *args)
{ dbior(args[0].sval,args[1].ival);}

/* dbhcr */
static const iocshFuncDef dbhcrFuncDef = {"dbhcr",0,0};
static void dbhcrCallFunc(const iocshArgBuf *args) { dbhcr();}

/* gft */
static const iocshArg gftArg0 = { "record name",iocshArgString};
static const iocshArg * const gftArgs[1] = {&gftArg0};
static const iocshFuncDef gftFuncDef = {"gft",1,gftArgs};
static void gftCallFunc(const iocshArgBuf *args) { gft(args[0].sval);}

/* pft */
static const iocshArg pftArg0 = { "record name",iocshArgString};
static const iocshArg pftArg1 = { "value",iocshArgString};
static const iocshArg * const pftArgs[2] = {&pftArg0,&pftArg1};
static const iocshFuncDef pftFuncDef = {"pft",2,pftArgs};
static void pftCallFunc(const iocshArgBuf *args)
{ pft(args[0].sval,args[1].sval);}

/* dbtpn */
static const iocshArg dbtpnArg0 = { "record name",iocshArgString};
static const iocshArg dbtpnArg1 = { "value",iocshArgString};
static const iocshArg * const dbtpnArgs[2] = {&dbtpnArg0,&dbtpnArg1};
static const iocshFuncDef dbtpnFuncDef = {"dbtpn",2,dbtpnArgs};
static void dbtpnCallFunc(const iocshArgBuf *args)
{ dbtpn(args[0].sval,args[1].sval);}

/* dbNotifyDump */
static const iocshFuncDef dbNotifyDumpFuncDef = {"dbNotifyDump",0,0};
static void dbNotifyDumpCallFunc(const iocshArgBuf *args) { dbNotifyDump();}

/* tpn */
static const iocshArg tpnArg0 = { "record name",iocshArgString};
static const iocshArg tpnArg1 = { "value",iocshArgString};
static const iocshArg * const tpnArgs[2] = {&tpnArg0,&tpnArg1};
static const iocshFuncDef tpnFuncDef = {"tpn",2,tpnArgs};
static void tpnCallFunc(const iocshArgBuf *args)
{ tpn(args[0].sval,args[1].sval);}

/* dblsr */
static const iocshArg dblsrArg0 = { "record name",iocshArgString};
static const iocshArg dblsrArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dblsrArgs[2] = {&dblsrArg0,&dblsrArg1};
static const iocshFuncDef dblsrFuncDef = {"dblsr",2,dblsrArgs};
static void dblsrCallFunc(const iocshArgBuf *args)
{ dblsr(args[0].sval,args[1].ival);}

/* dbLockShowLocked */
static const iocshArg dbLockShowLockedArg0 = { "interest level",iocshArgInt};
static const iocshArg * const dbLockShowLockedArgs[1] = {&dbLockShowLockedArg0};
static const iocshFuncDef dbLockShowLockedFuncDef =
    {"dbLockShowLocked",1,dbLockShowLockedArgs};
static void dbLockShowLockedCallFunc(const iocshArgBuf *args)
{ dbLockShowLocked(args[0].ival);}

/* scanOnceSetQueueSize */
static const iocshArg scanOnceSetQueueSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const scanOnceSetQueueSizeArgs[1] =
    {&scanOnceSetQueueSizeArg0};
static const iocshFuncDef scanOnceSetQueueSizeFuncDef =
    {"scanOnceSetQueueSize",1,scanOnceSetQueueSizeArgs};
static void scanOnceSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    scanOnceSetQueueSize(args[0].ival);
}

/* scanppl */
static const iocshArg scanpplArg0 = { "rate",iocshArgDouble};
static const iocshArg * const scanpplArgs[1] = {&scanpplArg0};
static const iocshFuncDef scanpplFuncDef = {"scanppl",1,scanpplArgs};
static void scanpplCallFunc(const iocshArgBuf *args)
{ scanppl(args[0].dval);}

/* scanpel */
static const iocshArg scanpelArg0 = { "event number",iocshArgInt};
static const iocshArg * const scanpelArgs[1] = {&scanpelArg0};
static const iocshFuncDef scanpelFuncDef = {"scanpel",1,scanpelArgs};
static void scanpelCallFunc(const iocshArgBuf *args)
{ scanpel(args[0].ival);}

/* scanpiol */
static const iocshFuncDef scanpiolFuncDef = {"scanpiol",0};
static void scanpiolCallFunc(const iocshArgBuf *args) { scanpiol();}

/* callbackSetQueueSize */
static const iocshArg callbackSetQueueSizeArg0 = { "bufsize",iocshArgInt};
static const iocshArg * const callbackSetQueueSizeArgs[1] =
    {&callbackSetQueueSizeArg0};
static const iocshFuncDef callbackSetQueueSizeFuncDef =
    {"callbackSetQueueSize",1,callbackSetQueueSizeArgs};
static void callbackSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    callbackSetQueueSize(args[0].ival);
}


void epicsShareAPI dbIocRegister(void)
{
    iocshRegister(&dbbFuncDef,dbbCallFunc);
    iocshRegister(&dbdFuncDef,dbdCallFunc);
    iocshRegister(&dbcFuncDef,dbcCallFunc);
    iocshRegister(&dbsFuncDef,dbsCallFunc);
    iocshRegister(&dbstatFuncDef,dbstatCallFunc);
    iocshRegister(&dbpFuncDef,dbpCallFunc);
    iocshRegister(&dbapFuncDef,dbapCallFunc);

    iocshRegister(&dbcarFuncDef,dbcarCallFunc);
    iocshRegister(&dbelFuncDef,dbelCallFunc);

    iocshRegister(&dbLoadDatabaseFuncDef,dbLoadDatabaseCallFunc);
    iocshRegister(&dbLoadRecordsFuncDef,dbLoadRecordsCallFunc);

    iocshRegister(&dbaFuncDef,dbaCallFunc);
    iocshRegister(&dblFuncDef,dblCallFunc);
    iocshRegister(&dbnrFuncDef,dbnrCallFunc);
    iocshRegister(&dblaFuncDef,dblaCallFunc);
    iocshRegister(&dbgrepFuncDef,dbgrepCallFunc);
    iocshRegister(&dbgfFuncDef,dbgfCallFunc);
    iocshRegister(&dbpfFuncDef,dbpfCallFunc);
    iocshRegister(&dbprFuncDef,dbprCallFunc);
    iocshRegister(&dbtrFuncDef,dbtrCallFunc);
    iocshRegister(&dbtgfFuncDef,dbtgfCallFunc);
    iocshRegister(&dbtpfFuncDef,dbtpfCallFunc);
    iocshRegister(&dbiorFuncDef,dbiorCallFunc);
    iocshRegister(&dbhcrFuncDef,dbhcrCallFunc);
    iocshRegister(&gftFuncDef,gftCallFunc);
    iocshRegister(&pftFuncDef,pftCallFunc);
    iocshRegister(&dbtpnFuncDef,dbtpnCallFunc);
    iocshRegister(&dbNotifyDumpFuncDef,dbNotifyDumpCallFunc);
    iocshRegister(&tpnFuncDef,tpnCallFunc);
    iocshRegister(&dblsrFuncDef,dblsrCallFunc);
    iocshRegister(&dbLockShowLockedFuncDef,dbLockShowLockedCallFunc);

    iocshRegister(&scanOnceSetQueueSizeFuncDef,scanOnceSetQueueSizeCallFunc);
    iocshRegister(&scanpplFuncDef,scanpplCallFunc);
    iocshRegister(&scanpelFuncDef,scanpelCallFunc);
    iocshRegister(&scanpiolFuncDef,scanpiolCallFunc);

    iocshRegister(&callbackSetQueueSizeFuncDef,callbackSetQueueSizeCallFunc);
}
