/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "iocsh.h"
#include "dbAccess.h"
#include "dbStaticIocRegister.h"
#include "registryIocRegister.h"
#include "asIocRegister.h"
#include "dbIocRegister.h"
#include "dbtoolsIocRegister.h"
#include "rsrvIocRegister.h"
#include "libComRegister.h"

#define epicsExportSharedSymbols
#include "miscIocRegister.h"
#include "iocshRegisterCommon.h"

void epicsShareAPI iocshRegisterCommon(void)
{
    iocshPpdbbase = &pdbbase;

    dbStaticIocRegister();
    registryIocRegister();
    dbIocRegister();
    dbtoolsIocRegister();
    asIocRegister();
    rsrvIocRegister();
    miscIocRegister();
    libComRegister();
}
