/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryDeviceSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include "registry.h"
#include "registryDeviceSupport.h"

static void *registryID = "device support";


DBCORE_API int registryDeviceSupportAdd(
    const char *name, const dset *pdset)
{
    return registryAdd(registryID, name, (void *)pdset);
}

DBCORE_API dset * registryDeviceSupportFind(
    const char *name)
{
    return registryFind(registryID, name);
}
