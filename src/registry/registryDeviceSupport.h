#ifndef INCregistryDeviceSupporth
#define INCregistryDeviceSupporth

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI registryDeviceSupportAdd(
    const char *name,struct dset *pdset);
epicsShareFunc struct dset * epicsShareAPI registryDeviceSupportFind(
    const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryDeviceSupporth */
