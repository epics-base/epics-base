#ifndef epicsFindSymbolh
#define epicsFindSymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* epicsFindSymbolh */
