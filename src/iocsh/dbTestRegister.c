/* dbTestRegister.c */
/* Author:  Marty Kraimer Date: 26APR2000 */

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
#include "dbTest.h"
#define epicsExportSharedSymbols
#include "dbTestRegister.h"

/* dba */
ioccrfArg dbaArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbaArgs[1] = {&dbaArg0};
ioccrfFuncDef dbaFuncDef = {"dba",1,dbaArgs};
void dbaCallFunc(ioccrfArg **args) { dba((char *)args[0]->value);}

/* dbl */
ioccrfArg dblArg0 = { "record type",ioccrfArgString,0};
ioccrfArg dblArg1 = { "output file",ioccrfArgString,0};
ioccrfArg dblArg2 = { "fields",ioccrfArgString,0};
ioccrfArg *dblArgs[3] = {&dblArg0,&dblArg1,&dblArg2};
ioccrfFuncDef dblFuncDef = {"dbl",3,dblArgs};
void dblCallFunc(ioccrfArg **args)
{
    dbl((char *)args[0]->value,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbnr */
ioccrfArg dbnrArg0 = { "verbose",ioccrfArgInt,0};
ioccrfArg *dbnrArgs[1] = {&dbnrArg0};
ioccrfFuncDef dbnrFuncDef = {"dbnr",1,dbnrArgs};
void dbnrCallFunc(ioccrfArg **args) { dbnr(*(int *)args[0]->value);}

/* dbgrep */
ioccrfArg dbgrepArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbgrepArgs[1] = {&dbgrepArg0};
ioccrfFuncDef dbgrepFuncDef = {"dbgrep",1,dbgrepArgs};
void dbgrepCallFunc(ioccrfArg **args) { dbgrep((char *)args[0]->value);}

/* dbgf */
ioccrfArg dbgfArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbgfArgs[1] = {&dbgfArg0};
ioccrfFuncDef dbgfFuncDef = {"dbgf",1,dbgfArgs};
void dbgfCallFunc(ioccrfArg **args) { dbgf((char *)args[0]->value);}

/* dbpf */
ioccrfArg dbpfArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbpfArg1 = { "value",ioccrfArgString,0};
ioccrfArg *dbpfArgs[2] = {&dbpfArg0,&dbpfArg1};
ioccrfFuncDef dbpfFuncDef = {"dbpf",2,dbpfArgs};
void dbpfCallFunc(ioccrfArg **args)
{ dbpf((char *)args[0]->value,(char *)args[1]->value);}

/* dbpr */
ioccrfArg dbprArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbprArg1 = { "interest_level",ioccrfArgInt,0};
ioccrfArg *dbprArgs[2] = {&dbprArg0,&dbprArg1};
ioccrfFuncDef dbprFuncDef = {"dbpr",2,dbprArgs};
void dbprCallFunc(ioccrfArg **args)
{ dbpr((char *)args[0]->value,*(int *)args[1]->value);}

/* dbtr */
ioccrfArg dbtrArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbtrArgs[1] = {&dbtrArg0};
ioccrfFuncDef dbtrFuncDef = {"dbtr",1,dbtrArgs};
void dbtrCallFunc(ioccrfArg **args) { dbtr((char *)args[0]->value);}

/* dbtgf */
ioccrfArg dbtgfArg0 = { "record name",ioccrfArgString,0};
ioccrfArg *dbtgfArgs[1] = {&dbtgfArg0};
ioccrfFuncDef dbtgfFuncDef = {"dbtgf",1,dbtgfArgs};
void dbtgfCallFunc(ioccrfArg **args) { dbtgf((char *)args[0]->value);}

/* dbtpf */
ioccrfArg dbtpfArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbtpfArg1 = { "value",ioccrfArgString,0};
ioccrfArg *dbtpfArgs[2] = {&dbtpfArg0,&dbtpfArg1};
ioccrfFuncDef dbtpfFuncDef = {"dbtpf",2,dbtpfArgs};
void dbtpfCallFunc(ioccrfArg **args)
{ dbtpf((char *)args[0]->value,(char *)args[1]->value);}

/* dbior */
ioccrfArg dbiorArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbiorArg1 = { "interest_level",ioccrfArgInt,0};
ioccrfArg *dbiorArgs[2] = {&dbiorArg0,&dbiorArg1};
ioccrfFuncDef dbiorFuncDef = {"dbior",2,dbiorArgs};
void dbiorCallFunc(ioccrfArg **args)
{ dbior((char *)args[0]->value,*(int *)args[1]->value);}

/* dbhcr */
ioccrfArg dbhcrArg0 = { "filename",ioccrfArgString,0};
ioccrfArg *dbhcrArgs[1] = {&dbhcrArg0};
ioccrfFuncDef dbhcrFuncDef = {"dbhcr",1,dbhcrArgs};
void dbhcrCallFunc(ioccrfArg **args) { dbhcr((char *)args[0]->value);}

void epicsShareAPI dbTestRegister(void)
{
    ioccrfRegister(&dbaFuncDef,dbaCallFunc);
    ioccrfRegister(&dblFuncDef,dblCallFunc);
    ioccrfRegister(&dbnrFuncDef,dbnrCallFunc);
    ioccrfRegister(&dbgrepFuncDef,dbgrepCallFunc);
    ioccrfRegister(&dbgfFuncDef,dbgfCallFunc);
    ioccrfRegister(&dbpfFuncDef,dbpfCallFunc);
    ioccrfRegister(&dbprFuncDef,dbprCallFunc);
    ioccrfRegister(&dbtrFuncDef,dbtrCallFunc);
    ioccrfRegister(&dbtgfFuncDef,dbtgfCallFunc);
    ioccrfRegister(&dbtpfFuncDef,dbtpfCallFunc);
    ioccrfRegister(&dbiorFuncDef,dbiorCallFunc);
    ioccrfRegister(&dbhcrFuncDef,dbhcrCallFunc);
}
