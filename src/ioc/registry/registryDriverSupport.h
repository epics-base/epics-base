/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_registryDriverSupport_H
#define INC_registryDriverSupport_H

#include "drvSup.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI registryDriverSupportAdd(
    const char *name, struct drvet *pdrvet);
epicsShareFunc struct drvet * epicsShareAPI registryDriverSupportFind(
    const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INC_registryDriverSupport_H */
