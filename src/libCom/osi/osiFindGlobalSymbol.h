#ifndef osiFindGlobalSymbolh
#define osiFindGlobalSymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void * epicsShareAPI osiFindGlobalSymbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* osiFindGlobalSymbolh */
