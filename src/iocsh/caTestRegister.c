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
static ioccrfArg casrArg0 = { "level",ioccrfArgInt,0};
static ioccrfArg *casrArgs[1] = {&casrArg0};
static ioccrfFuncDef casrFuncDef = {"casr",1,casrArgs};
static void casrCallFunc(ioccrfArg **args)
{
    casr(*(int *)args[0]->value);
}

/* dbel */
static ioccrfArg dbelArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbelArg1 = { "level",ioccrfArgInt,0};
static ioccrfArg *dbelArgs[2] = {&dbelArg0,&dbelArg1};
static ioccrfFuncDef dbelFuncDef = {"dbel",2,dbelArgs};
static void dbelCallFunc(ioccrfArg **args)
{
    dbel((char *)args[0]->value, *(int *)args[1]->value);
}


void epicsShareAPI caTestRegister(void)
{
    ioccrfRegister(&casrFuncDef,casrCallFunc);
    ioccrfRegister(&dbelFuncDef,dbelCallFunc);
}
