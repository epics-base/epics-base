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

#include "ioccrf.h"
#include "rsrv.h"
#define epicsExportSharedSymbols
#include "caTestRegister.h"

/* casr */
ioccrfArg casrArg0 = { "level",ioccrfArgInt,0};
ioccrfArg *casrArgs[1] = {&casrArg0};
ioccrfFuncDef casrFuncDef = {"casr",1,casrArgs};
void casrCallFunc(ioccrfArg **args)
{
    casr(*(int *)args[0]->value);
}


void epicsShareAPI caTestRegister(void)
{
    ioccrfRegister(&casrFuncDef,casrCallFunc);
}
