/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_registryCommon_H
#define INC_registryCommon_H

#include "dbStaticLib.h"
#include "devSup.h"
#include "dbJLink.h"
#include "registryRecordType.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void registerRecordTypes(
    DBBASE *pbase, int nRecordTypes,
    const char * const *recordTypeNames, const recordTypeLocation *rtl);
epicsShareFunc void registerDevices(
    DBBASE *pbase, int nDevices,
    const char * const *deviceSupportNames, const dset * const *devsl);
epicsShareFunc void registerDrivers(
    DBBASE *pbase, int nDrivers,
    const char * const *driverSupportNames, struct drvet * const *drvsl);
epicsShareFunc void registerJLinks(
    DBBASE *pbase, int nDrivers, jlif * const *jlifsl);

#ifdef __cplusplus
}
#endif

#endif /* INC_registryCommon_H */
