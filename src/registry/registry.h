#ifndef INCregistryh
#define INCregistryh

#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TABLE_SIZE 1024

epicsShareFunc int epicsShareAPI registryAdd(
    void *registryID,const char *name,void *data);
epicsShareFunc void *epicsShareAPI registryFind(
    void *registryID,const char *name);

epicsShareFunc int epicsShareAPI registrySetTableSize(int size);
epicsShareFunc void epicsShareAPI registryFree();
epicsShareFunc int epicsShareAPI registryDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INCregistryh */
