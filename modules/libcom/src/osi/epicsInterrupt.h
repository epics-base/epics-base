/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef epicsInterrupth
#define epicsInterrupth

#ifdef __cplusplus
extern "C" {
#endif

#include "libComAPI.h"

LIBCOM_API int epicsInterruptLock(void);
LIBCOM_API void epicsInterruptUnlock(int key);
LIBCOM_API int epicsInterruptIsInterruptContext(void);
LIBCOM_API void epicsInterruptContextMessage(const char *message);

#ifdef __cplusplus
}
#endif

#include "osdInterrupt.h"

#endif /* epicsInterrupth */
