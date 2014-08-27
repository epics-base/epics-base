/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011
 */ 

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
