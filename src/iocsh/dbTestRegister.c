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

#include "dbTest.h"
#include "db_test.h"
#include "dbLock.h"
#include "dbScan.h"
#define epicsExportSharedSymbols
#include "dbTestRegister.h"
#include "ioccrf.h"

/* dba */
static ioccrfArg dbaArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbaArgs[1] = {&dbaArg0};
static ioccrfFuncDef dbaFuncDef = {"dba",1,dbaArgs};
static void dbaCallFunc(ioccrfArg **args) { dba((char *)args[0]->value);}

/* dbl */
static ioccrfArg dblArg0 = { "record type",ioccrfArgString,0};
static ioccrfArg dblArg1 = { "output file",ioccrfArgString,0};
static ioccrfArg dblArg2 = { "fields",ioccrfArgString,0};
static ioccrfArg *dblArgs[3] = {&dblArg0,&dblArg1,&dblArg2};
static ioccrfFuncDef dblFuncDef = {"dbl",3,dblArgs};
static void dblCallFunc(ioccrfArg **args)
{
    dbl((char *)args[0]->value,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbnr */
static ioccrfArg dbnrArg0 = { "verbose",ioccrfArgInt,0};
static ioccrfArg *dbnrArgs[1] = {&dbnrArg0};
static ioccrfFuncDef dbnrFuncDef = {"dbnr",1,dbnrArgs};
static void dbnrCallFunc(ioccrfArg **args) { dbnr(*(int *)args[0]->value);}

/* dbgrep */
static ioccrfArg dbgrepArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbgrepArgs[1] = {&dbgrepArg0};
static ioccrfFuncDef dbgrepFuncDef = {"dbgrep",1,dbgrepArgs};
static void dbgrepCallFunc(ioccrfArg **args) { dbgrep((char *)args[0]->value);}

/* dbgf */
static ioccrfArg dbgfArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbgfArgs[1] = {&dbgfArg0};
static ioccrfFuncDef dbgfFuncDef = {"dbgf",1,dbgfArgs};
static void dbgfCallFunc(ioccrfArg **args) { dbgf((char *)args[0]->value);}

/* dbpf */
static ioccrfArg dbpfArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbpfArg1 = { "value",ioccrfArgString,0};
static ioccrfArg *dbpfArgs[2] = {&dbpfArg0,&dbpfArg1};
static ioccrfFuncDef dbpfFuncDef = {"dbpf",2,dbpfArgs};
static void dbpfCallFunc(ioccrfArg **args)
{ dbpf((char *)args[0]->value,(char *)args[1]->value);}

/* dbpr */
static ioccrfArg dbprArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbprArg1 = { "interest_level",ioccrfArgInt,0};
static ioccrfArg *dbprArgs[2] = {&dbprArg0,&dbprArg1};
static ioccrfFuncDef dbprFuncDef = {"dbpr",2,dbprArgs};
static void dbprCallFunc(ioccrfArg **args)
{ dbpr((char *)args[0]->value,*(int *)args[1]->value);}

/* dbtr */
static ioccrfArg dbtrArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbtrArgs[1] = {&dbtrArg0};
static ioccrfFuncDef dbtrFuncDef = {"dbtr",1,dbtrArgs};
static void dbtrCallFunc(ioccrfArg **args) { dbtr((char *)args[0]->value);}

/* dbtgf */
static ioccrfArg dbtgfArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *dbtgfArgs[1] = {&dbtgfArg0};
static ioccrfFuncDef dbtgfFuncDef = {"dbtgf",1,dbtgfArgs};
static void dbtgfCallFunc(ioccrfArg **args) { dbtgf((char *)args[0]->value);}

/* dbtpf */
static ioccrfArg dbtpfArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbtpfArg1 = { "value",ioccrfArgString,0};
static ioccrfArg *dbtpfArgs[2] = {&dbtpfArg0,&dbtpfArg1};
static ioccrfFuncDef dbtpfFuncDef = {"dbtpf",2,dbtpfArgs};
static void dbtpfCallFunc(ioccrfArg **args)
{ dbtpf((char *)args[0]->value,(char *)args[1]->value);}

/* dbior */
static ioccrfArg dbiorArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbiorArg1 = { "interest_level",ioccrfArgInt,0};
static ioccrfArg *dbiorArgs[2] = {&dbiorArg0,&dbiorArg1};
static ioccrfFuncDef dbiorFuncDef = {"dbior",2,dbiorArgs};
static void dbiorCallFunc(ioccrfArg **args)
{ dbior((char *)args[0]->value,*(int *)args[1]->value);}

/* dbhcr */
static ioccrfArg dbhcrArg0 = { "filename",ioccrfArgString,0};
static ioccrfArg *dbhcrArgs[1] = {&dbhcrArg0};
static ioccrfFuncDef dbhcrFuncDef = {"dbhcr",1,dbhcrArgs};
static void dbhcrCallFunc(ioccrfArg **args) { dbhcr((char *)args[0]->value);}

/* gft */
static ioccrfArg gftArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg *gftArgs[1] = {&gftArg0};
static ioccrfFuncDef gftFuncDef = {"gft",1,gftArgs};
static void gftCallFunc(ioccrfArg **args) { gft((char *)args[0]->value);}

/* pft */
static ioccrfArg pftArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg pftArg1 = { "value",ioccrfArgString,0};
static ioccrfArg *pftArgs[2] = {&pftArg0,&pftArg1};
static ioccrfFuncDef pftFuncDef = {"pft",2,pftArgs};
static void pftCallFunc(ioccrfArg **args)
{ pft((char *)args[0]->value,(char *)args[1]->value);}

/* tpn */
static ioccrfArg tpnArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg tpnArg1 = { "value",ioccrfArgString,0};
static ioccrfArg *tpnArgs[2] = {&tpnArg0,&tpnArg1};
static ioccrfFuncDef tpnFuncDef = {"tpn",2,tpnArgs};
static void tpnCallFunc(ioccrfArg **args)
{ tpn((char *)args[0]->value,(char *)args[1]->value);}

/* dblsr */
static ioccrfArg dblsrArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dblsrArg1 = { "interest_level",ioccrfArgInt,0};
static ioccrfArg *dblsrArgs[2] = {&dblsrArg0,&dblsrArg1};
static ioccrfFuncDef dblsrFuncDef = {"dblsr",2,dblsrArgs};
static void dblsrCallFunc(ioccrfArg **args)
{ dblsr((char *)args[0]->value,*(int *)args[1]->value);}

/* scanppl */
static ioccrfArg scanpplArg0 = { "rate",ioccrfArgDouble,0};
static ioccrfArg *scanpplArgs[1] = {&scanpplArg0};
static ioccrfFuncDef scanpplFuncDef = {"scanppl",1,scanpplArgs};
static void scanpplCallFunc(ioccrfArg **args)
{ scanppl(*(double *)args[0]->value);}

/* scanpel */
static ioccrfArg scanpelArg0 = { "event_number",ioccrfArgInt,0};
static ioccrfArg *scanpelArgs[1] = {&scanpelArg0};
static ioccrfFuncDef scanpelFuncDef = {"scanpel",1,scanpelArgs};
static void scanpelCallFunc(ioccrfArg **args)
{ scanpel(*(int *)args[0]->value);}

/* scanpiol */
static ioccrfFuncDef scanpiolFuncDef = {"scanpiol",0,0};
static void scanpiolCallFunc(ioccrfArg **args) { scanpiol();}

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
    ioccrfRegister(&gftFuncDef,gftCallFunc);
    ioccrfRegister(&pftFuncDef,pftCallFunc);
    ioccrfRegister(&tpnFuncDef,tpnCallFunc);
    ioccrfRegister(&dblsrFuncDef,dblsrCallFunc);
    ioccrfRegister(&scanpplFuncDef,scanpplCallFunc);
    ioccrfRegister(&scanpelFuncDef,scanpelCallFunc);
    ioccrfRegister(&scanpiolFuncDef,scanpiolCallFunc);
}
