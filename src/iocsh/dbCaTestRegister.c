/* dbCaTestRegister.c */
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

#include "dbCaTest.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "dbCaTestRegister.h"

/* dbcar */
static ioccrfArg dbcarArg0 = { "record name",ioccrfArgString,0};
static ioccrfArg dbcarArg1 = { "level",ioccrfArgInt,0};
static ioccrfArg *dbcarArgs[2] = {&dbcarArg0,&dbcarArg1};
static ioccrfFuncDef dbcarFuncDef = {"dbcar",2,dbcarArgs};
static void dbcarCallFunc(ioccrfArg **args)
{
    dbcar((char *)args[0]->value,*(int *)args[1]->value);
}


void epicsShareAPI dbCaTestRegister(void)
{
    ioccrfRegister(&dbcarFuncDef,dbcarCallFunc);
}
