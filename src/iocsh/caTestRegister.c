/* caTestRegister.c */
/* Author:  Marty Kraimer Date: 01MAY2000 */

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

#include "rsrv.h"
#include "dbEvent.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "caTestRegister.h"

/* casr */
static const ioccrfArg casrArg0 = { "level",ioccrfArgInt};
static const ioccrfArg *casrArgs[1] = {&casrArg0};
static const ioccrfFuncDef casrFuncDef = {"casr",1,casrArgs};
static void casrCallFunc(const ioccrfArgBuf *args)
{
    casr(args[0].ival);
}

/* dbel */
static const ioccrfArg dbelArg0 = { "record name",ioccrfArgString};
static const ioccrfArg dbelArg1 = { "level",ioccrfArgInt};
static const ioccrfArg *dbelArgs[2] = {&dbelArg0,&dbelArg1};
static const ioccrfFuncDef dbelFuncDef = {"dbel",2,dbelArgs};
static void dbelCallFunc(const ioccrfArgBuf *args)
{
    dbel(args[0].sval, args[1].ival);
}


void epicsShareAPI caTestRegister(void)
{
    ioccrfRegister(&casrFuncDef,casrCallFunc);
    ioccrfRegister(&dbelFuncDef,dbelCallFunc);
}
