/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryDriverSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryDriverSupport.h"

static void *registryID = "driver support";


epicsShareFunc int epicsShareAPI registryDriverSupportAdd(
    const char *name, struct drvet *pdrvet)
{
    return registryAdd(registryID, name, pdrvet);
}

epicsShareFunc struct drvet * epicsShareAPI registryDriverSupportFind(
    const char *name)
{
    return registryFind(registryID, name);
}
