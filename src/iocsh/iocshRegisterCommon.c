/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocshRegisterCommon.c */
/* Author:  Marty Kraimer Date: 26APR2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dbAccess.h>

#define epicsExportSharedSymbols
#include "iocCoreLimitsRegister.h"
#include "dbStaticRegister.h"
#include "iocUtilRegister.h"
#include "dbTestRegister.h"
#include "dbBkptRegister.h"
#include "dbCaTestRegister.h"
#include "caTestRegister.h"
#include "dbAccessRegister.h"
#include "asTestRegister.h"
#include "envRegister.h"
#include "iocshRegisterCommon.h"
#include "osiRegister.h"
#include "registryRegister.h"
#include "iocsh.h"

void epicsShareAPI iocshRegisterCommon(void)
{
    iocshPpdbbase = &pdbbase;
    iocCoreLimitsRegister();
    osiRegister();
    iocUtilRegister();
    dbStaticRegister();
    dbTestRegister();
    dbBkptRegister();
    dbCaTestRegister();
    caTestRegister();
    dbAccessRegister();
    asTestRegister();
    envRegister();
    registryRegister();
}
