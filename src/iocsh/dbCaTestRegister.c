/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCaTestRegister.c */
/* Author:  Marty Kraimer Date: 01MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsStdio.h"
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
