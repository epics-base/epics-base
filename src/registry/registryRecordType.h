/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCregistryRecordTypeh
#define INCregistryRecordTypeh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbRecordType;
struct rset;
struct dbBase;

typedef int (*computeSizeOffset)(struct dbRecordType *pdbRecordType);

typedef struct recordTypeLocation {
    struct rset *prset;
    computeSizeOffset sizeOffset;
}recordTypeLocation;
    

epicsShareFunc int epicsShareAPI registryRecordTypeAdd(
    const char *name,const recordTypeLocation *prtl);
epicsShareFunc recordTypeLocation * epicsShareAPI registryRecordTypeFind(
    const char *name);

int registerRecordDeviceDriver(struct dbBase *pdbbase);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryRecordTypeh */
