#ifndef INCregistryDriverSupporth
#define INCregistryDriverSupporth

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* c interface definitions */
epicsShareFunc int epicsShareAPI registryDriverSupportAdd(
    const char *name,struct drvet *pdrvet);
epicsShareFunc struct drvet * epicsShareAPI registryDriverSupportFind(
    const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryDriverSupporth */
