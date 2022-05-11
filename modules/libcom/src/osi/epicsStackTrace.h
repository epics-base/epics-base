/*
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * SPDX-License-Identifier: EPICS
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */

/**
 * \file epicsStackTrace.h
 *
 * Functions for printing the stack trace
 */
#ifndef INC_epicsStackTrace_H
#define INC_epicsStackTrace_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Dump a stack trace
 *
 * Dump a stack trace to the errlog.
 */
LIBCOM_API void epicsStackTrace(void);

/* Inquire about functionality implemented on your system */

/** Bit mask to check if stack trace provides numerical addresses.*/
#define EPICS_STACKTRACE_ADDRESSES   (1<<0)

/** Bit mask to check if stack trace is able to lookup dynamic symbols.*/
#define EPICS_STACKTRACE_DYN_SYMBOLS (1<<1)

/** Bit mask to check if stack trace is able to lookup global symbols  */
#define EPICS_STACKTRACE_GBL_SYMBOLS (1<<2)

/** Bit mask to check if stack trace is able to lookup local symbols   */
#define EPICS_STACKTRACE_LCL_SYMBOLS (1<<3)

/** \brief Get supported stacktrace features 
 *
 * Returns an ORed bitset of supported features.  Use the EPICS_STACKTRACE_ masks to 
 * check if a feature is supported.
 *
 * \return 0 if getting the stack trace is unsupported.  Otherwise
 * returns an ORed bitset of supported features. 
 */
LIBCOM_API int epicsStackTraceGetFeatures(void);

#ifdef __cplusplus
}
#endif

#endif
