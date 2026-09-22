#ifndef INC_registerModules_H
#define INC_registerModules_H

#include "libComAPI.h"

LIBCOM_API void registerPrintModules();
LIBCOM_API int registerModule(const char *name, const char *version);

#endif /* INC_registerModules_H */
