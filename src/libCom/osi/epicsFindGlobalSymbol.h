#ifndef epicsFindGlobalSymbolh
#define epicsFindGlobalSymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void * epicsShareAPI epicsFindGlobalSymbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* epicsFindGlobalSymbolh */
