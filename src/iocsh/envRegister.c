/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* envRegister.c */
/* Author:  Marty Kraimer Date: 19MAY2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsStdio.h"
#include "envDefs.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "envRegister.h"

/* epicsPrtEnvParams */
static const iocshFuncDef epicsPrtEnvParamsFuncDef = {"epicsPrtEnvParams",0,0};
static void epicsPrtEnvParamsCallFunc(const iocshArgBuf *args) { epicsPrtEnvParams();}

void epicsShareAPI envRegister(void)
{
    iocshRegister(&epicsPrtEnvParamsFuncDef,epicsPrtEnvParamsCallFunc);
}
