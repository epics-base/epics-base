#ifndef INCregistryFunctionh
#define INCregistryFunctionh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*REGISTRYFUNCTION)(void);

/* c interface definitions */
epicsShareFunc int epicsShareAPI registryFunctionAdd(
    const char *name,REGISTRYFUNCTION func);
epicsShareFunc REGISTRYFUNCTION epicsShareAPI registryFunctionFind(
    const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryFunctionh */
