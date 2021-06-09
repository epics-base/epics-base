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

#include "callback.h"
#include "dbAccess.h"
#include "dbBkpt.h"
#include "dbCaTest.h"
#include "dbEvent.h"
#include "dbIocRegister.h"
#include "dbJLink.h"
#include "dbLock.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbServer.h"
#include "dbState.h"
#include "db_test.h"
#include "dbTest.h"

DBCORE_API extern int callbackParallelThreadsDefault;

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
    iocshSetError(dbLoadDatabase(args[0].sval,args[1].sval,args[2].sval));
}

/* dbLoadRecords */
static const iocshArg dbLoadRecordsArg0 = { "file name",iocshArgString};
static const iocshArg dbLoadRecordsArg1 = { "substitutions",iocshArgString};
static const iocshArg * const dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
static const iocshFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
static void dbLoadRecordsCallFunc(const iocshArgBuf *args)
{
    iocshSetError(dbLoadRecords(args[0].sval,args[1].sval));
}

/* dbb */
static const iocshArg dbbArg0 = { "record name",iocshArgString};
static const iocshArg * const dbbArgs[1] = {&dbbArg0};
static const iocshFuncDef dbbFuncDef = {"dbb",1,dbbArgs,
                                        "Add breakpoint to a lock set.\n"};
static void dbbCallFunc(const iocshArgBuf *args) { dbb(args[0].sval);}

/* dbd */
static const iocshArg dbdArg0 = { "record name",iocshArgString};
static const iocshArg * const dbdArgs[1] = {&dbdArg0};
static const iocshFuncDef dbdFuncDef = {"dbd",1,dbdArgs,
                                        "Remove breakpoint from a record.\n"};
static void dbdCallFunc(const iocshArgBuf *args) { dbd(args[0].sval);}

/* dbc */
static const iocshArg dbcArg0 = { "record name",iocshArgString};
static const iocshArg * const dbcArgs[1] = {&dbcArg0};
static const iocshFuncDef dbcFuncDef = {"dbc",1,dbcArgs,
                                        "Continue processing in a lock set.\n"};
static void dbcCallFunc(const iocshArgBuf *args) { dbc(args[0].sval);}

/* dbs */
static const iocshArg dbsArg0 = { "record name",iocshArgString};
static const iocshArg * const dbsArgs[1] = {&dbsArg0};
static const iocshFuncDef dbsFuncDef = {"dbs",1,dbsArgs,
                                        "Step through record processing.\n"};
static void dbsCallFunc(const iocshArgBuf *args) { dbs(args[0].sval);}

/* dbstat */
static const iocshFuncDef dbstatFuncDef = {"dbstat",0,0,
                                           "print list of stopped records, and breakpoints set in locksets.\n"};
static void dbstatCallFunc(const iocshArgBuf *args) { dbstat();}

/* dbp */
static const iocshArg dbpArg0 = { "record name",iocshArgString};
static const iocshArg dbpArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbpArgs[2] = {&dbpArg0,&dbpArg1};
static const iocshFuncDef dbpFuncDef = {"dbp",2,dbpArgs,
                                        "print stopped record.\n"};
static void dbpCallFunc(const iocshArgBuf *args)
{ dbp(args[0].sval,args[1].ival);}

/* dbap */
static const iocshArg dbapArg0 = { "record name",iocshArgString};
static const iocshArg * const dbapArgs[1] = {&dbapArg0};
static const iocshFuncDef dbapFuncDef = {"dbap",1,dbapArgs,
                                         "toggle printing after processing a certain record.\n"};
static void dbapCallFunc(const iocshArgBuf *args) { dbap(args[0].sval);}

/* dbsr */
static const iocshArg dbsrArg0 = { "interest level",iocshArgInt};
static const iocshArg * const dbsrArgs[1] = {&dbsrArg0};
static const iocshFuncDef dbsrFuncDef = {"dbsr",1,dbsrArgs,
                                         "Database Server Report.\n"};
static void dbsrCallFunc(const iocshArgBuf *args) { dbsr(args[0].ival);}

/* dbcar */
static const iocshArg dbcarArg0 = { "record name",iocshArgString};
static const iocshArg dbcarArg1 = { "level",iocshArgInt};
static const iocshArg * const dbcarArgs[2] = {&dbcarArg0,&dbcarArg1};
static const iocshFuncDef dbcarFuncDef = {"dbcar",2,dbcarArgs,
                                          "Database Channel Access Report.\n"
                                          "Shows status of Channel Access links (CA_LINK).\n"
                                          "interest level 0 - Shows statistics for all links.\n"
                                          "               1 - Shows info. of only disconnected links.\n"
                                          "               2 - Shows info. for all links.\n"};
static void dbcarCallFunc(const iocshArgBuf *args)
{
    dbcar(args[0].sval,args[1].ival);
}

/* dbjlr */
static const iocshArg dbjlrArg0 = { "record name",iocshArgString};
static const iocshArg dbjlrArg1 = { "level",iocshArgInt};
static const iocshArg * const dbjlrArgs[2] = {&dbjlrArg0,&dbjlrArg1};
static const iocshFuncDef dbjlrFuncDef = {"dbjlr",2,dbjlrArgs,
                                          "Database JSON link Report.\n"};
static void dbjlrCallFunc(const iocshArgBuf *args)
{
    dbjlr(args[0].sval,args[1].ival);
}

/* dbel */
static const iocshArg dbelArg0 = { "record name",iocshArgString};
static const iocshArg dbelArg1 = { "level",iocshArgInt};
static const iocshArg * const dbelArgs[2] = {&dbelArg0,&dbelArg1};
static const iocshFuncDef dbelFuncDef = {"dbel",2,dbelArgs,
                                         "Database event list.\n"
                                         "Show information on dbEvent subscriptions.\n"};
static void dbelCallFunc(const iocshArgBuf *args)
{
    dbel(args[0].sval, args[1].ival);
}

/* dba */
static const iocshArg dbaArg0 = { "record name",iocshArgString};
static const iocshArg * const dbaArgs[1] = {&dbaArg0};
static const iocshFuncDef dbaFuncDef = {"dba",1,dbaArgs,
                                        "dbAddr info.\n"};
static void dbaCallFunc(const iocshArgBuf *args) { dba(args[0].sval);}

/* dbl */
static const iocshArg dblArg0 = { "record type",iocshArgString};
static const iocshArg dblArg1 = { "fields",iocshArgString};
static const iocshArg * const dblArgs[] = {&dblArg0,&dblArg1};
static const iocshFuncDef dblFuncDef = {"dbl",2,dblArgs,
                                        "Database list.\n"
                                        "List record/field names.\n"
                                        "With no arguments, lists all record names.\n"};
static void dblCallFunc(const iocshArgBuf *args)
{
    dbl(args[0].sval,args[1].sval);
}

/* dbnr */
static const iocshArg dbnrArg0 = { "verbose",iocshArgInt};
static const iocshArg * const dbnrArgs[1] = {&dbnrArg0};
static const iocshFuncDef dbnrFuncDef = {"dbnr",1,dbnrArgs,
                                         "List stats on record alias()s.\n"};
static void dbnrCallFunc(const iocshArgBuf *args) { dbnr(args[0].ival);}

/* dbli */
static const iocshArg dbliArg0 = { "pattern",iocshArgString};
static const iocshArg * const dbliArgs[1] = {&dbliArg0};
static const iocshFuncDef dbliFuncDef = {"dbli",1,dbliArgs,
                                        "List info() tags with names matching pattern.\n"};
static void dbliCallFunc(const iocshArgBuf *args) { dbli(args[0].sval);}

/* dbla */
static const iocshArg dblaArg0 = { "pattern",iocshArgString};
static const iocshArg * const dblaArgs[1] = {&dblaArg0};
static const iocshFuncDef dblaFuncDef = {"dbla",1,dblaArgs,
                                         "List record alias()s by alias name pattern.\n"};
static void dblaCallFunc(const iocshArgBuf *args) { dbla(args[0].sval);}

/* dbgrep */
static const iocshArg dbgrepArg0 = { "pattern",iocshArgString};
static const iocshArg * const dbgrepArgs[1] = {&dbgrepArg0};
static const iocshFuncDef dbgrepFuncDef = {"dbgrep",1,dbgrepArgs,
                                           "List record names matching pattern.\n"};
static void dbgrepCallFunc(const iocshArgBuf *args) { dbgrep(args[0].sval);}

/* dbgf */
static const iocshArg dbgfArg0 = { "record name",iocshArgString};
static const iocshArg * const dbgfArgs[1] = {&dbgfArg0};
static const iocshFuncDef dbgfFuncDef = {"dbgf",1,dbgfArgs,
                                         "Database Get Field.\n"
                                         "Print current value of record field.\n"};
static void dbgfCallFunc(const iocshArgBuf *args) { dbgf(args[0].sval);}

/* dbpf */
static const iocshArg dbpfArg0 = { "record name",iocshArgString};
static const iocshArg dbpfArg1 = { "value",iocshArgString};
static const iocshArg * const dbpfArgs[2] = {&dbpfArg0,&dbpfArg1};
static const iocshFuncDef dbpfFuncDef = {"dbpf",2,dbpfArgs,
                                         "Database Put Field.\n"
                                         "Change value of record field.\n"};
static void dbpfCallFunc(const iocshArgBuf *args)
{ dbpf(args[0].sval,args[1].sval);}

/* dbpr */
static const iocshArg dbprArg0 = { "record name",iocshArgString};
static const iocshArg dbprArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbprArgs[2] = {&dbprArg0,&dbprArg1};
static const iocshFuncDef dbprFuncDef = {"dbpr",2,dbprArgs,
                                         "Database Print Record.\n"
                                         "Print values of record fields.\n"};
static void dbprCallFunc(const iocshArgBuf *args)
{ dbpr(args[0].sval,args[1].ival);}

/* dbtr */
static const iocshArg dbtrArg0 = { "record name",iocshArgString};
static const iocshArg * const dbtrArgs[1] = {&dbtrArg0};
static const iocshFuncDef dbtrFuncDef = {"dbtr",1,dbtrArgs,
                                         "Process record and then some fields.\n"};
static void dbtrCallFunc(const iocshArgBuf *args) { dbtr(args[0].sval);}

/* dbtgf */
static const iocshArg dbtgfArg0 = { "record name",iocshArgString};
static const iocshArg * const dbtgfArgs[1] = {&dbtgfArg0};
static const iocshFuncDef dbtgfFuncDef = {"dbtgf",1,dbtgfArgs,
                                          "Database Test Get Field.\n"
                                          "Get field with different DBR_* types"};
static void dbtgfCallFunc(const iocshArgBuf *args) { dbtgf(args[0].sval);}

/* dbtpf */
static const iocshArg dbtpfArg0 = { "record name",iocshArgString};
static const iocshArg dbtpfArg1 = { "value",iocshArgString};
static const iocshArg * const dbtpfArgs[2] = {&dbtpfArg0,&dbtpfArg1};
static const iocshFuncDef dbtpfFuncDef = {"dbtpf",2,dbtpfArgs,
                                          "Database Test Put Field.\n"};
static void dbtpfCallFunc(const iocshArgBuf *args)
{ dbtpf(args[0].sval,args[1].sval);}

/* dbior */
static const iocshArg dbiorArg0 = { "driver name",iocshArgString};
static const iocshArg dbiorArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dbiorArgs[] = {&dbiorArg0,&dbiorArg1};
static const iocshFuncDef dbiorFuncDef = {"dbior",2,dbiorArgs,
                                          "Driver Report.\n"};
static void dbiorCallFunc(const iocshArgBuf *args)
{ dbior(args[0].sval,args[1].ival);}

/* dbhcr */
static const iocshFuncDef dbhcrFuncDef = {"dbhcr",0,0,
                                          "Database Report Device Config.\n"};
static void dbhcrCallFunc(const iocshArgBuf *args) { dbhcr();}

/* gft */
static const iocshArg gftArg0 = { "record name",iocshArgString};
static const iocshArg * const gftArgs[1] = {&gftArg0};
static const iocshFuncDef gftFuncDef = {"gft",1,gftArgs,
                                        "Report dbChannel info and value.\n"};
static void gftCallFunc(const iocshArgBuf *args) { gft(args[0].sval);}

/* pft */
static const iocshArg pftArg0 = { "record name",iocshArgString};
static const iocshArg pftArg1 = { "value",iocshArgString};
static const iocshArg * const pftArgs[2] = {&pftArg0,&pftArg1};
static const iocshFuncDef pftFuncDef = {"pft",2,pftArgs,
                                        "dbChannel put value.\n"};
static void pftCallFunc(const iocshArgBuf *args)
{ pft(args[0].sval,args[1].sval);}

/* dbtpn */
static const iocshArg dbtpnArg0 = { "record name",iocshArgString};
static const iocshArg dbtpnArg1 = { "value",iocshArgString};
static const iocshArg * const dbtpnArgs[2] = {&dbtpnArg0,&dbtpnArg1};
static const iocshFuncDef dbtpnFuncDef = {"dbtpn",2,dbtpnArgs,
                                          "Database Put Notify\n"
                                          "Without value, begin async. processing and get\n"
                                          "With value, begin put, process, and get"};
static void dbtpnCallFunc(const iocshArgBuf *args)
{ dbtpn(args[0].sval,args[1].sval);}

/* dbNotifyDump */
static const iocshFuncDef dbNotifyDumpFuncDef = {"dbNotifyDump",0,0,
                                                 "Report status of any active async processing with completion notification.\n"};
static void dbNotifyDumpCallFunc(const iocshArgBuf *args) { dbNotifyDump();}

/* dbPutAttribute */
static const iocshArg dbPutAttrArg0 = { "record type",iocshArgString};
static const iocshArg dbPutAttrArg1 = { "attribute name",iocshArgString};
static const iocshArg dbPutAttrArg2 = { "value",iocshArgString};
static const iocshArg * const dbPutAttrArgs[] =
    {&dbPutAttrArg0, &dbPutAttrArg1, &dbPutAttrArg2};
static const iocshFuncDef dbPutAttrFuncDef =
    {"dbPutAttribute",3,dbPutAttrArgs,
     "Set/Create record attribute.\n"};
static void dbPutAttrCallFunc(const iocshArgBuf *args)
{ dbPutAttribute(args[0].sval,args[1].sval,args[2].sval);}

/* tpn */
static const iocshArg tpnArg0 = { "record name",iocshArgString};
static const iocshArg tpnArg1 = { "value",iocshArgString};
static const iocshArg * const tpnArgs[2] = {&tpnArg0,&tpnArg1};
static const iocshFuncDef tpnFuncDef = {"tpn",2,tpnArgs,
                                        "Begin async. process and get.\n"};
static void tpnCallFunc(const iocshArgBuf *args)
{ tpn(args[0].sval,args[1].sval);}

/* dblsr */
static const iocshArg dblsrArg0 = { "record name",iocshArgString};
static const iocshArg dblsrArg1 = { "interest level",iocshArgInt};
static const iocshArg * const dblsrArgs[2] = {&dblsrArg0,&dblsrArg1};
static const iocshFuncDef dblsrFuncDef = {"dblsr",2,dblsrArgs,
                                          "Database Lockset report.\n"};
static void dblsrCallFunc(const iocshArgBuf *args)
{ dblsr(args[0].sval,args[1].ival);}

/* dbLockShowLocked */
static const iocshArg dbLockShowLockedArg0 = { "interest level",iocshArgInt};
static const iocshArg * const dbLockShowLockedArgs[1] = {&dbLockShowLockedArg0};
static const iocshFuncDef dbLockShowLockedFuncDef =
    {"dbLockShowLocked",1,dbLockShowLockedArgs,
     "Show Locksets which are currently locked.\n"};
static void dbLockShowLockedCallFunc(const iocshArgBuf *args)
{ dbLockShowLocked(args[0].ival);}

/* scanOnceSetQueueSize */
static const iocshArg scanOnceSetQueueSizeArg0 = { "size",iocshArgInt};
static const iocshArg * const scanOnceSetQueueSizeArgs[1] =
    {&scanOnceSetQueueSizeArg0};
static const iocshFuncDef scanOnceSetQueueSizeFuncDef =
    {"scanOnceSetQueueSize",1,scanOnceSetQueueSizeArgs,
     "Change size of Scan once queue.\n"
     "Must be called before iocInit().\n"};
static void scanOnceSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    scanOnceSetQueueSize(args[0].ival);
}

/* scanOnceQueueShow */
static const iocshArg scanOnceQueueShowArg0 = { "reset",iocshArgInt};
static const iocshArg * const scanOnceQueueShowArgs[1] =
    {&scanOnceQueueShowArg0};
static const iocshFuncDef scanOnceQueueShowFuncDef =
    {"scanOnceQueueShow",1,scanOnceQueueShowArgs,
     "Show details and statitics of scan once queue processing.\n"};
static void scanOnceQueueShowCallFunc(const iocshArgBuf *args)
{
    scanOnceQueueShow(args[0].ival);
}

/* scanppl */
static const iocshArg scanpplArg0 = { "rate",iocshArgDouble};
static const iocshArg * const scanpplArgs[1] = {&scanpplArg0};
static const iocshFuncDef scanpplFuncDef = {"scanppl",1,scanpplArgs,
                                            "print periodic scan lists.\n"};
static void scanpplCallFunc(const iocshArgBuf *args)
{ scanppl(args[0].dval);}

/* scanpel */
static const iocshArg scanpelArg0 = { "event name",iocshArgString};
static const iocshArg * const scanpelArgs[1] = {&scanpelArg0};
static const iocshFuncDef scanpelFuncDef = {"scanpel",1,scanpelArgs,
                                            "Print info for records with SCAN = \"Event\".\n"};
static void scanpelCallFunc(const iocshArgBuf *args)
{ scanpel(args[0].sval);}

/* postEvent */
static const iocshArg postEventArg0 = { "event name",iocshArgString};
static const iocshArg * const postEventArgs[1] = {&postEventArg0};
static const iocshFuncDef postEventFuncDef = {"postEvent",1,postEventArgs,
                                              "Manually scan all records with EVNT == name.\n"};
static void postEventCallFunc(const iocshArgBuf *args)
{
    EVENTPVT pel = eventNameToHandle(args[0].sval);
    postEvent(pel);
}

/* scanpiol */
static const iocshFuncDef scanpiolFuncDef = {"scanpiol",0,0,
                                             "Print info for records with SCAN = \"I/O Intr\".\n"};
static void scanpiolCallFunc(const iocshArgBuf *args) { scanpiol();}

/* callbackSetQueueSize */
static const iocshArg callbackSetQueueSizeArg0 = { "bufsize",iocshArgInt};
static const iocshArg * const callbackSetQueueSizeArgs[1] =
    {&callbackSetQueueSizeArg0};
static const iocshFuncDef callbackSetQueueSizeFuncDef =
    {"callbackSetQueueSize",1,callbackSetQueueSizeArgs,
     "Change depth of queue for callback workers.\n"
     "Must be called before iocInit().\n"};
static void callbackSetQueueSizeCallFunc(const iocshArgBuf *args)
{
    callbackSetQueueSize(args[0].ival);
}

/* callbackQueueShow */
static const iocshArg callbackQueueShowArg0 = { "reset", iocshArgInt};
static const iocshArg * const callbackQueueShowArgs[1] =
    {&callbackQueueShowArg0};
static const iocshFuncDef callbackQueueShowFuncDef =
    {"callbackQueueShow",1,callbackQueueShowArgs,
     "Show status of callback thread processing queue.\n"};
static void callbackQueueShowCallFunc(const iocshArgBuf *args)
{
    callbackQueueShow(args[0].ival);
}

/* callbackParallelThreads */
static const iocshArg callbackParallelThreadsArg0 = { "no of threads", iocshArgInt};
static const iocshArg callbackParallelThreadsArg1 = { "priority", iocshArgString};
static const iocshArg * const callbackParallelThreadsArgs[2] =
    {&callbackParallelThreadsArg0,&callbackParallelThreadsArg1};
static const iocshFuncDef callbackParallelThreadsFuncDef =
    {"callbackParallelThreads",2,callbackParallelThreadsArgs,
     "Configure multiple workers for a given callback queue priority level.\n"
     "priority may be omitted or \"*\" to act on all priorities\n"
     "or one of LOW, MEDIUM, or HIGH.\n"};
static void callbackParallelThreadsCallFunc(const iocshArgBuf *args)
{
    callbackParallelThreads(args[0].ival, args[1].sval);
}

/* dbStateCreate */
static const iocshArg dbStateArgName = { "name", iocshArgString };
static const iocshArg * const dbStateCreateArgs[] = { &dbStateArgName };
static const iocshFuncDef dbStateCreateFuncDef = { "dbStateCreate", 1, dbStateCreateArgs,
                                                   "Allocate new state name for \"state\" filter.\n"};
static void dbStateCreateCallFunc (const iocshArgBuf *args)
{
    dbStateCreate(args[0].sval);
}

/* dbStateSet */
static const iocshArg * const dbStateSetArgs[] = { &dbStateArgName };
static const iocshFuncDef dbStateSetFuncDef = { "dbStateSet", 1, dbStateSetArgs,
                                                "Change state to set for \"state\" filter.\n"};
static void dbStateSetCallFunc (const iocshArgBuf *args)
{
    dbStateId sid = dbStateFind(args[0].sval);

    if (sid)
        dbStateSet(sid);
}

/* dbStateClear */
static const iocshArg * const dbStateClearArgs[] = { &dbStateArgName };
static const iocshFuncDef dbStateClearFuncDef = { "dbStateClear", 1, dbStateClearArgs,
                                                  "Change state to clear for \"state\" filter.\n" };
static void dbStateClearCallFunc (const iocshArgBuf *args)
{
    dbStateId sid = dbStateFind(args[0].sval);

    if (sid)
        dbStateClear(sid);
}

/* dbStateShow */
static const iocshArg dbStateShowArg1 = { "level", iocshArgInt };
static const iocshArg * const dbStateShowArgs[] = { &dbStateArgName, &dbStateShowArg1 };
static const iocshFuncDef dbStateShowFuncDef = { "dbStateShow", 2, dbStateShowArgs,
                                                 "Show set/clear status of named state. (cf. \"state\" filter)\n" };
static void dbStateShowCallFunc (const iocshArgBuf *args)
{
    dbStateId sid = dbStateFind(args[0].sval);

    if (sid)
        dbStateShow(sid, args[1].ival);
}

/* dbStateShowAll */
static const iocshArg dbStateShowAllArg0 = { "level", iocshArgInt };
static const iocshArg * const dbStateShowAllArgs[] = { &dbStateShowAllArg0 };
static const iocshFuncDef dbStateShowAllFuncDef = { "dbStateShowAll", 1, dbStateShowAllArgs,
                                                    "Show set/clear status of all named states. (cf. \"state\" filter)\n"  };
static void dbStateShowAllCallFunc (const iocshArgBuf *args)
{
    dbStateShowAll(args[0].ival);
}

void dbIocRegister(void)
{
    iocshRegister(&dbbFuncDef,dbbCallFunc);
    iocshRegister(&dbdFuncDef,dbdCallFunc);
    iocshRegister(&dbcFuncDef,dbcCallFunc);
    iocshRegister(&dbsFuncDef,dbsCallFunc);
    iocshRegister(&dbstatFuncDef,dbstatCallFunc);
    iocshRegister(&dbpFuncDef,dbpCallFunc);
    iocshRegister(&dbapFuncDef,dbapCallFunc);

    iocshRegister(&dbsrFuncDef,dbsrCallFunc);
    iocshRegister(&dbcarFuncDef,dbcarCallFunc);
    iocshRegister(&dbelFuncDef,dbelCallFunc);
    iocshRegister(&dbjlrFuncDef,dbjlrCallFunc);

    iocshRegister(&dbLoadDatabaseFuncDef,dbLoadDatabaseCallFunc);
    iocshRegister(&dbLoadRecordsFuncDef,dbLoadRecordsCallFunc);

    iocshRegister(&dbaFuncDef,dbaCallFunc);
    iocshRegister(&dblFuncDef,dblCallFunc);
    iocshRegister(&dbnrFuncDef,dbnrCallFunc);
    iocshRegister(&dblaFuncDef,dblaCallFunc);
    iocshRegister(&dbliFuncDef,dbliCallFunc);
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
    iocshRegister(&dbPutAttrFuncDef,dbPutAttrCallFunc);
    iocshRegister(&tpnFuncDef,tpnCallFunc);
    iocshRegister(&dblsrFuncDef,dblsrCallFunc);
    iocshRegister(&dbLockShowLockedFuncDef,dbLockShowLockedCallFunc);

    iocshRegister(&scanOnceSetQueueSizeFuncDef,scanOnceSetQueueSizeCallFunc);
    iocshRegister(&scanOnceQueueShowFuncDef,scanOnceQueueShowCallFunc);
    iocshRegister(&scanpplFuncDef,scanpplCallFunc);
    iocshRegister(&scanpelFuncDef,scanpelCallFunc);
    iocshRegister(&postEventFuncDef,postEventCallFunc);
    iocshRegister(&scanpiolFuncDef,scanpiolCallFunc);

    iocshRegister(&callbackSetQueueSizeFuncDef,callbackSetQueueSizeCallFunc);
    iocshRegister(&callbackQueueShowFuncDef,callbackQueueShowCallFunc);
    iocshRegister(&callbackParallelThreadsFuncDef,callbackParallelThreadsCallFunc);

    /* Needed before callback system is initialized */
    callbackParallelThreadsDefault = epicsThreadGetCPUs();

    iocshRegister(&dbStateCreateFuncDef, dbStateCreateCallFunc);
    iocshRegister(&dbStateSetFuncDef, dbStateSetCallFunc);
    iocshRegister(&dbStateClearFuncDef, dbStateClearCallFunc);
    iocshRegister(&dbStateShowFuncDef, dbStateShowCallFunc);
    iocshRegister(&dbStateShowAllFuncDef, dbStateShowAllCallFunc);
}
