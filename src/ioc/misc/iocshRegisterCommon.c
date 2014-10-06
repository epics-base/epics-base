/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "iocsh.h"
#include "libComRegister.h"

#define epicsExportSharedSymbols
#include "asIocRegister.h"
#include "dbAccess.h"
#include "dbIocRegister.h"
#include "dbStaticIocRegister.h"
#include "dbtoolsIocRegister.h"
#include "iocshRegisterCommon.h"
#include "miscIocRegister.h"
#include "registryIocRegister.h"
#include "rsrvIocRegister.h"

void iocshRegisterCommon(void)
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
