#ifndef INC_epicsStacktrace_H
#define INC_epicsStacktrace_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dump a stacktrace to the errlog */
epicsShareFunc void epicsStackTrace(void);

#ifdef __cplusplus
}
#endif

#endif
