/* dbBkptRegister.c */
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

#include "ellLib.h"
#include "osiThread.h"
#include "epicsEvent.h"
#include "tsStamp.h"
#include "errlog.h"
#include "alarm.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "dbBkpt.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "dbBkptRegister.h"

/* dbb */
static const ioccrfArg dbbArg0 = { "record name",ioccrfArgString};
static const ioccrfArg * const dbbArgs[1] = {&dbbArg0};
static const ioccrfFuncDef dbbFuncDef = {"dbb",1,dbbArgs};
static void dbbCallFunc(const ioccrfArgBuf *args) { dbb(args[0].sval);}

/* dbd */
static const ioccrfArg dbdArg0 = { "record name",ioccrfArgString};
static const ioccrfArg * const dbdArgs[1] = {&dbdArg0};
static const ioccrfFuncDef dbdFuncDef = {"dbd",1,dbdArgs};
static void dbdCallFunc(const ioccrfArgBuf *args) { dbd(args[0].sval);}

/* dbc */
static const ioccrfArg dbcArg0 = { "record name",ioccrfArgString};
static const ioccrfArg * const dbcArgs[1] = {&dbcArg0};
static const ioccrfFuncDef dbcFuncDef = {"dbc",1,dbcArgs};
static void dbcCallFunc(const ioccrfArgBuf *args) { dbc(args[0].sval);}

/* dbs */
static const ioccrfArg dbsArg0 = { "record name",ioccrfArgString};
static const ioccrfArg * const dbsArgs[1] = {&dbsArg0};
static const ioccrfFuncDef dbsFuncDef = {"dbs",1,dbsArgs};
static void dbsCallFunc(const ioccrfArgBuf *args) { dbs(args[0].sval);}

static const ioccrfFuncDef dbstatFuncDef = {"dbstat",0};
static void dbstatCallFunc(const ioccrfArgBuf *args) { dbstat();}

/* dbp */
static const ioccrfArg dbpArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbpArg1 = { "interest_level",ioccrfArgInt};
static const ioccrfArg * const dbpArgs[2] = {&dbpArg0,&dbpArg1};
static const ioccrfFuncDef dbpFuncDef = {"dbp",2,dbpArgs};
static void dbpCallFunc(const ioccrfArgBuf *args)
{ dbp(args[0].sval,args[1].ival);}

/* dbap */
static const ioccrfArg dbapArg0 = { "record name",ioccrfArgString};
static const ioccrfArg * const dbapArgs[1] = {&dbapArg0};
static const ioccrfFuncDef dbapFuncDef = {"dbap",1,dbapArgs};
static void dbapCallFunc(const ioccrfArgBuf *args) { dbap(args[0].sval);}

void epicsShareAPI dbBkptRegister(void)
{
    ioccrfRegister(&dbbFuncDef,dbbCallFunc);
    ioccrfRegister(&dbdFuncDef,dbdCallFunc);
    ioccrfRegister(&dbcFuncDef,dbcCallFunc);
    ioccrfRegister(&dbsFuncDef,dbsCallFunc);
    ioccrfRegister(&dbstatFuncDef,dbstatCallFunc);
    ioccrfRegister(&dbpFuncDef,dbpCallFunc);
    ioccrfRegister(&dbapFuncDef,dbapCallFunc);
}
