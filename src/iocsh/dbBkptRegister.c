/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbBkptRegister.c */
/* Author:  Marty Kraimer Date: 02MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsStdio.h"
#include "ellLib.h"
#include "errlog.h"
#include "alarm.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "dbBkpt.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "dbBkptRegister.h"

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

void epicsShareAPI dbBkptRegister(void)
{
    iocshRegister(&dbbFuncDef,dbbCallFunc);
    iocshRegister(&dbdFuncDef,dbdCallFunc);
    iocshRegister(&dbcFuncDef,dbcCallFunc);
    iocshRegister(&dbsFuncDef,dbsCallFunc);
    iocshRegister(&dbstatFuncDef,dbstatCallFunc);
    iocshRegister(&dbpFuncDef,dbpCallFunc);
    iocshRegister(&dbapFuncDef,dbapCallFunc);
}
