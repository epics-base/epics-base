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
#include "ellLib.h"
#include "dbNotify.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "dbTestRegister.h"

/* dba */
static const ioccrfArg dbaArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *dbaArgs[1] = {&dbaArg0};
static const ioccrfFuncDef dbaFuncDef = {"dba",1,dbaArgs};
static void dbaCallFunc(const ioccrfArgBuf *args) { dba(args[0].sval);}

/* dbl */
static const ioccrfArg dblArg0 = { "record type",ioccrfArgString};
static const ioccrfArg dblArg1 = { "output file",ioccrfArgString};
static const ioccrfArg dblArg2 = { "fields",ioccrfArgString};
static const ioccrfArg *dblArgs[3] = {&dblArg0,&dblArg1,&dblArg2};
static const ioccrfFuncDef dblFuncDef = {"dbl",3,dblArgs};
static void dblCallFunc(const ioccrfArgBuf *args)
{
    dbl(args[0].sval,args[1].sval,args[2].sval);
}

/* dbnr */
static const ioccrfArg dbnrArg0 = { "verbose",ioccrfArgInt};
static const ioccrfArg *dbnrArgs[1] = {&dbnrArg0};
static const ioccrfFuncDef dbnrFuncDef = {"dbnr",1,dbnrArgs};
static void dbnrCallFunc(const ioccrfArgBuf *args) { dbnr(args[0].ival);}

/* dbgrep */
static const ioccrfArg dbgrepArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *dbgrepArgs[1] = {&dbgrepArg0};
static const ioccrfFuncDef dbgrepFuncDef = {"dbgrep",1,dbgrepArgs};
static void dbgrepCallFunc(const ioccrfArgBuf *args) { dbgrep(args[0].sval);}

/* dbgf */
static const ioccrfArg dbgfArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *dbgfArgs[1] = {&dbgfArg0};
static const ioccrfFuncDef dbgfFuncDef = {"dbgf",1,dbgfArgs};
static void dbgfCallFunc(const ioccrfArgBuf *args) { dbgf(args[0].sval);}

/* dbpf */
static const ioccrfArg dbpfArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbpfArg1 = { "value",ioccrfArgString};
static const ioccrfArg *dbpfArgs[2] = {&dbpfArg0,&dbpfArg1};
static const ioccrfFuncDef dbpfFuncDef = {"dbpf",2,dbpfArgs};
static void dbpfCallFunc(const ioccrfArgBuf *args)
{ dbpf(args[0].sval,args[1].sval);}

/* dbpr */
static const ioccrfArg dbprArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbprArg1 = { "interest_level",ioccrfArgInt};
static const ioccrfArg *dbprArgs[2] = {&dbprArg0,&dbprArg1};
static const ioccrfFuncDef dbprFuncDef = {"dbpr",2,dbprArgs};
static void dbprCallFunc(const ioccrfArgBuf *args)
{ dbpr(args[0].sval,args[1].ival);}

/* dbtr */
static const ioccrfArg dbtrArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *dbtrArgs[1] = {&dbtrArg0};
static const ioccrfFuncDef dbtrFuncDef = {"dbtr",1,dbtrArgs};
static void dbtrCallFunc(const ioccrfArgBuf *args) { dbtr(args[0].sval);}

/* dbtgf */
static const ioccrfArg dbtgfArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *dbtgfArgs[1] = {&dbtgfArg0};
static const ioccrfFuncDef dbtgfFuncDef = {"dbtgf",1,dbtgfArgs};
static void dbtgfCallFunc(const ioccrfArgBuf *args) { dbtgf(args[0].sval);}

/* dbtpf */
static const ioccrfArg dbtpfArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbtpfArg1 = { "value",ioccrfArgString};
static const ioccrfArg *dbtpfArgs[2] = {&dbtpfArg0,&dbtpfArg1};
static const ioccrfFuncDef dbtpfFuncDef = {"dbtpf",2,dbtpfArgs};
static void dbtpfCallFunc(const ioccrfArgBuf *args)
{ dbtpf(args[0].sval,args[1].sval);}

/* dbior */
static const ioccrfArg dbiorArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbiorArg1 = { "interest_level",ioccrfArgInt};
static const ioccrfArg *dbiorArgs[2] = {&dbiorArg0,&dbiorArg1};
static const ioccrfFuncDef dbiorFuncDef = {"dbior",2,dbiorArgs};
static void dbiorCallFunc(const ioccrfArgBuf *args)
{ dbior(args[0].sval,args[1].ival);}

/* dbhcr */
static const ioccrfArg dbhcrArg0 = { "filename",ioccrfArgString};
static const ioccrfArg *dbhcrArgs[1] = {&dbhcrArg0};
static const ioccrfFuncDef dbhcrFuncDef = {"dbhcr",1,dbhcrArgs};
static void dbhcrCallFunc(const ioccrfArgBuf *args) { dbhcr(args[0].sval);}

/* gft */
static const ioccrfArg gftArg0 = { "record name",ioccrfArgString};
static const ioccrfArg *gftArgs[1] = {&gftArg0};
static const ioccrfFuncDef gftFuncDef = {"gft",1,gftArgs};
static void gftCallFunc(const ioccrfArgBuf *args) { gft(args[0].sval);}

/* pft */
static const ioccrfArg pftArg0 = { "record name",ioccrfArgString};
static const ioccrfArg pftArg1 = { "value",ioccrfArgString};
static const ioccrfArg *pftArgs[2] = {&pftArg0,&pftArg1};
static const ioccrfFuncDef pftFuncDef = {"pft",2,pftArgs};
static void pftCallFunc(const ioccrfArgBuf *args)
{ pft(args[0].sval,args[1].sval);}

/* dbtpn */
static const ioccrfArg dbtpnArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbtpnArg1 = { "value",ioccrfArgString};
static const ioccrfArg *dbtpnArgs[2] = {&dbtpnArg0,&dbtpnArg1};
static const ioccrfFuncDef dbtpnFuncDef = {"dbtpn",2,dbtpnArgs};
static void dbtpnCallFunc(const ioccrfArgBuf *args)
{ dbtpn(args[0].sval,args[1].sval);}

/* tpn */
static const ioccrfArg tpnArg0 = { "record name",ioccrfArgString};
static const ioccrfArg tpnArg1 = { "value",ioccrfArgString};
static const ioccrfArg *tpnArgs[2] = {&tpnArg0,&tpnArg1};
static const ioccrfFuncDef tpnFuncDef = {"tpn",2,tpnArgs};
static void tpnCallFunc(const ioccrfArgBuf *args)
{ tpn(args[0].sval,args[1].sval);}

/* dblsr */
static const ioccrfArg dblsrArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dblsrArg1 = { "interest_level",ioccrfArgInt};
static const ioccrfArg *dblsrArgs[2] = {&dblsrArg0,&dblsrArg1};
static const ioccrfFuncDef dblsrFuncDef = {"dblsr",2,dblsrArgs};
static void dblsrCallFunc(const ioccrfArgBuf *args)
{ dblsr(args[0].sval,args[1].ival);}

/* scanppl */
static const ioccrfArg scanpplArg0 = { "rate",ioccrfArgDouble};
static const ioccrfArg *scanpplArgs[1] = {&scanpplArg0};
static const ioccrfFuncDef scanpplFuncDef = {"scanppl",1,scanpplArgs};
static void scanpplCallFunc(const ioccrfArgBuf *args)
{ scanppl(args[0].dval);}

/* scanpel */
static const ioccrfArg scanpelArg0 = { "event_number",ioccrfArgInt};
static const ioccrfArg *scanpelArgs[1] = {&scanpelArg0};
static const ioccrfFuncDef scanpelFuncDef = {"scanpel",1,scanpelArgs};
static void scanpelCallFunc(const ioccrfArgBuf *args)
{ scanpel(args[0].ival);}

/* scanpiol */
static const ioccrfFuncDef scanpiolFuncDef = {"scanpiol",0};
static void scanpiolCallFunc(const ioccrfArgBuf *args) { scanpiol();}

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
    ioccrfRegister(&dbtpnFuncDef,dbtpnCallFunc);
    ioccrfRegister(&tpnFuncDef,tpnCallFunc);
    ioccrfRegister(&dblsrFuncDef,dblsrCallFunc);
    ioccrfRegister(&scanpplFuncDef,scanpplCallFunc);
    ioccrfRegister(&scanpelFuncDef,scanpelCallFunc);
    ioccrfRegister(&scanpiolFuncDef,scanpiolCallFunc);
}
