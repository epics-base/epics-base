/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#ifndef INC_epicsStackTrace_H
#define INC_epicsStackTrace_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dump a stack trace to the errlog */
epicsShareFunc void epicsStackTrace(void);

/* Inquire about functionality implemented on your system */

/* StackTrace provides numerical addresses      */
#define EPICS_STACKTRACE_ADDRESSES   (1<<0)

/* StackTrace is able to lookup dynamic symbols */
#define EPICS_STACKTRACE_DYN_SYMBOLS (1<<1)

/* StackTrace is able to lookup global symbols  */
#define EPICS_STACKTRACE_GBL_SYMBOLS (1<<2)

/* StackTrace is able to lookup local symbols   */
#define EPICS_STACKTRACE_LCL_SYMBOLS (1<<3)

/* returns ORed bitset of supported features    */
epicsShareFunc int epicsStackTraceGetFeatures(void);

#ifdef __cplusplus
}
#endif

#endif
