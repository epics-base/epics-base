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
#include "iocsh.h"
#include "dbCaTestRegister.h"

/* dbcar */
static const iocshArg dbcarArg0 = { "record name",iocshArgString};
static const iocshArg dbcarArg1 = { "level",iocshArgInt};
static const iocshArg * const dbcarArgs[2] = {&dbcarArg0,&dbcarArg1};
static const iocshFuncDef dbcarFuncDef = {"dbcar",2,dbcarArgs};
static void dbcarCallFunc(const iocshArgBuf *args)
{
    dbcar(args[0].sval,args[1].ival);
}


void epicsShareAPI dbCaTestRegister(void)
{
    iocshRegister(&dbcarFuncDef,dbcarCallFunc);
}
