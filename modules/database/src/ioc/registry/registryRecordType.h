/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_registryRecordType_H
#define INC_registryRecordType_H

#include "recSup.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbRecordType;
struct dbBase;

typedef int (*computeSizeOffset)(struct dbRecordType *pdbRecordType);

typedef struct recordTypeLocation {
    struct typed_rset *prset;
    computeSizeOffset sizeOffset;
}recordTypeLocation;

epicsShareFunc int registryRecordTypeAdd(
    const char *name, const recordTypeLocation *prtl);
epicsShareFunc recordTypeLocation * registryRecordTypeFind(
    const char *name);

int registerRecordDeviceDriver(struct dbBase *pdbbase);

#ifdef __cplusplus
}
#endif


#endif /* INC_registryRecordType_H */
