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

#include "ioccrf.h"
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
#define epicsExportSharedSymbols
#include "dbBkptRegister.h"

/* dbb */
ioccrfArg dbbArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbbArgs[1] = {&dbbArg0};
ioccrfFuncDef dbbFuncDef = {"dbb",1,dbbArgs};
void dbbCallFunc(ioccrfArg **args) { dbb((char *)args[0]->value);}

/* dbd */
ioccrfArg dbdArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbdArgs[1] = {&dbdArg0};
ioccrfFuncDef dbdFuncDef = {"dbd",1,dbdArgs};
void dbdCallFunc(ioccrfArg **args) { dbd((char *)args[0]->value);}

/* dbc */
ioccrfArg dbcArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbcArgs[1] = {&dbcArg0};
ioccrfFuncDef dbcFuncDef = {"dbc",1,dbcArgs};
void dbcCallFunc(ioccrfArg **args) { dbc((char *)args[0]->value);}

/* dbs */
ioccrfArg dbsArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbsArgs[1] = {&dbsArg0};
ioccrfFuncDef dbsFuncDef = {"dbs",1,dbsArgs};
void dbsCallFunc(ioccrfArg **args) { dbs((char *)args[0]->value);}

ioccrfFuncDef dbstatFuncDef = {"dbstat",0,0};
void dbstatCallFunc(ioccrfArg **args) { dbstat();}

/* dbp */
ioccrfArg dbpArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbpArg1 = { "interest_level",ioccrfArgInt,0};
ioccrfArg *dbpArgs[2] = {&dbpArg0,&dbpArg1};
ioccrfFuncDef dbpFuncDef = {"dbp",2,dbpArgs};
void dbpCallFunc(ioccrfArg **args)
{ dbp((char *)args[0]->value,*(int *)args[1]->value);}

/* dbap */
ioccrfArg dbapArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbapArgs[1] = {&dbapArg0};
ioccrfFuncDef dbapFuncDef = {"dbap",1,dbapArgs};
void dbapCallFunc(ioccrfArg **args) { dbap((char *)args[0]->value);}

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
