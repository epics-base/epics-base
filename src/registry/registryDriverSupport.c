/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* registryDriverSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#include "dbBase.h"
#include "drvSup.h"
#define epicsExportSharedSymbols
#include "registry.h"
#include "registryDriverSupport.h"

const char *driverSupport = "driver support";
static void *registryID = (void *)&driverSupport;


epicsShareFunc int epicsShareAPI registryDriverSupportAdd(
    const char *name,struct drvet *pdrvet)
{
    return(registryAdd(registryID,name,(void *)pdrvet));
}

epicsShareFunc struct drvet * epicsShareAPI registryDriverSupportFind(
    const char *name)
{
    return((struct drvet *)registryFind(registryID,name));
}
