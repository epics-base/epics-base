/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryDeviceSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryDeviceSupport.h"

static void *registryID = "device support";


epicsShareFunc int epicsShareAPI registryDeviceSupportAdd(
    const char *name, const struct dset *pdset)
{
    return registryAdd(registryID, name, (void *)pdset);
}

epicsShareFunc struct dset * epicsShareAPI registryDeviceSupportFind(
    const char *name)
{
    return registryFind(registryID, name);
}
