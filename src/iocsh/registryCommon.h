/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* registryCommon.h */

#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "devSup.h"
#include "drvSup.h"
#include "recSup.h"
#include "registryRecordType.h"
#include "iocsh.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI registerRecordTypes(
    DBBASE *pbase, int nRecordTypes,
    const char* const* recordTypeNames, const recordTypeLocation *rtl);
epicsShareFunc void epicsShareAPI registerDevices(
    DBBASE *pbase, int nDevices,
    const char* const* deviceSupportNames, const dset* const* devsl);
epicsShareFunc void epicsShareAPI registerDrivers(
    DBBASE *pbase, int nDrivers,
    const char* const* driverSupportNames, struct drvet* const* drvsl);

#ifdef __cplusplus
}
#endif
