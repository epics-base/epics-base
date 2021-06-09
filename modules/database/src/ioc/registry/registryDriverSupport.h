/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_registryDriverSupport_H
#define INC_registryDriverSupport_H

#include "drvSup.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API int registryDriverSupportAdd(
    const char *name, struct drvet *pdrvet);
DBCORE_API struct drvet * registryDriverSupportFind(
    const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INC_registryDriverSupport_H */
