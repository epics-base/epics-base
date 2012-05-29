/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* registryCommon.c */

/* Author:  Andrew Johnson
 * Date:    2004-03-19
 */

#include "errlog.h"

#define epicsExportSharedSymbols
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "registryCommon.h"


void epicsShareAPI registerRecordTypes(DBBASE *pbase, int nRecordTypes,
    const char * const *recordTypeNames, const recordTypeLocation *rtl)
{
    int i;
    for (i = 0; i < nRecordTypes; i++) {
        recordTypeLocation *precordTypeLocation;
        computeSizeOffset sizeOffset;
        DBENTRY dbEntry;

        if (registryRecordTypeFind(recordTypeNames[i])) continue;
        if (!registryRecordTypeAdd(recordTypeNames[i], &rtl[i])) {
            errlogPrintf("registryRecordTypeAdd failed %s\n",
                recordTypeNames[i]);
            continue;
        }
        dbInitEntry(pbase,&dbEntry);
        precordTypeLocation = registryRecordTypeFind(recordTypeNames[i]);
        sizeOffset = precordTypeLocation->sizeOffset;
        if (dbFindRecordType(&dbEntry, recordTypeNames[i])) {
            errlogPrintf("registerRecordDeviceDriver failed %s\n",
                recordTypeNames[i]);
        } else {
            sizeOffset(dbEntry.precordType);
        }
    }
}

void epicsShareAPI registerDevices(DBBASE *pbase, int nDevices,
    const char * const *deviceSupportNames, const dset * const *devsl)
{
    int i;
    for (i = 0; i < nDevices; i++) {
        if (registryDeviceSupportFind(deviceSupportNames[i])) continue;
        if (!registryDeviceSupportAdd(deviceSupportNames[i], devsl[i])) {
            errlogPrintf("registryDeviceSupportAdd failed %s\n",
                deviceSupportNames[i]);
            continue;
        }
    }
}

void epicsShareAPI registerDrivers(DBBASE *pbase, int nDrivers,
    const char * const * driverSupportNames, struct drvet * const *drvsl)
{
    int i;
    for (i = 0; i < nDrivers; i++) {
        if (registryDriverSupportFind(driverSupportNames[i])) continue;
        if (!registryDriverSupportAdd(driverSupportNames[i], drvsl[i])) {
            errlogPrintf("registryDriverSupportAdd failed %s\n",
                driverSupportNames[i]);
            continue;
        }
    }
}

