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
