/* dbCaTestRegister.h */
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
#include "dbCaTest.h"
#define epicsExportSharedSymbols
#include "dbCaTestRegister.h"

/* dbcar */
ioccrfArg dbcarArg0 = { "record name",ioccrfArgString,0};
ioccrfArg dbcarArg1 = { "level",ioccrfArgInt,0};
ioccrfArg *dbcarArgs[2] = {&dbcarArg0,&dbcarArg1};
ioccrfFuncDef dbcarFuncDef = {"dbcar",2,dbcarArgs};
void dbcarCallFunc(ioccrfArg **args)
{
    dbcar((char *)args[0]->value,*(char *)args[1]->value);
}


void epicsShareAPI dbCaTestRegister(void)
{
    ioccrfRegister(&dbcarFuncDef,dbcarCallFunc);
}
