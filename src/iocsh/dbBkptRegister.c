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
#include "osiSem.h"
#include "tsStamp.h"
#include "errlog.h"
#include "alarm.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "dbBkpt.h"
#include "ioccrf.h"
#define epicsExportSharedSymbols
#include "dbBkptRegister.h"

/* dbb */
static ioccrfArg dbbArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbbArgs[1] = {&dbbArg0};
static ioccrfFuncDef dbbFuncDef = {"dbb",1,dbbArgs};
static void dbbCallFunc(ioccrfArg **args) { dbb((char *)args[0]->value);}

/* dbd */
static ioccrfArg dbdArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbdArgs[1] = {&dbdArg0};
static ioccrfFuncDef dbdFuncDef = {"dbd",1,dbdArgs};
static void dbdCallFunc(ioccrfArg **args) { dbd((char *)args[0]->value);}

/* dbc */
static ioccrfArg dbcArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbcArgs[1] = {&dbcArg0};
static ioccrfFuncDef dbcFuncDef = {"dbc",1,dbcArgs};
static void dbcCallFunc(ioccrfArg **args) { dbc((char *)args[0]->value);}

/* dbs */
static ioccrfArg dbsArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbsArgs[1] = {&dbsArg0};
static ioccrfFuncDef dbsFuncDef = {"dbs",1,dbsArgs};
static void dbsCallFunc(ioccrfArg **args) { dbs((char *)args[0]->value);}

static ioccrfFuncDef dbstatFuncDef = {"dbstat",0,0};
static void dbstatCallFunc(ioccrfArg **args) { dbstat();}

/* dbp */
static ioccrfArg dbpArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbpArg1 = { "interest_level",ioccrfArgInt,0};
static ioccrfArg *dbpArgs[2] = {&dbpArg0,&dbpArg1};
static ioccrfFuncDef dbpFuncDef = {"dbp",2,dbpArgs};
static void dbpCallFunc(ioccrfArg **args)
{ dbp((char *)args[0]->value,*(int *)args[1]->value);}

/* dbap */
static ioccrfArg dbapArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbapArgs[1] = {&dbapArg0};
static ioccrfFuncDef dbapFuncDef = {"dbap",1,dbapArgs};
static void dbapCallFunc(ioccrfArg **args) { dbap((char *)args[0]->value);}

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
