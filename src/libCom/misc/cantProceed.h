#ifndef INCcantProceedh
#define INCcantProceedh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void epicsShareAPI cantProceed(const char *errorMessage);
epicsShareFunc void * epicsShareAPI callocMustSucceed(size_t count, size_t size, const char *errorMessage);
epicsShareFunc void * epicsShareAPI mallocMustSucceed(size_t size, const char *errorMessage);
#ifdef __cplusplus
}
#endif

#endif /* cantProceedh */
