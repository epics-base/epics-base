/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryDriverSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include "registry.h"
#include "registryDriverSupport.h"

static void *registryID = "driver support";


DBCORE_API int registryDriverSupportAdd(
    const char *name, struct drvet *pdrvet)
{
    return registryAdd(registryID, name, pdrvet);
}

DBCORE_API struct drvet * registryDriverSupportFind(
    const char *name)
{
    return registryFind(registryID, name);
}
