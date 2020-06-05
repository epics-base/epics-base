/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2008 Diamond Light Source Ltd
* Copyright (c) 2004 Oak Ridge National Laboratory
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_generalTimeSup_H
#define INC_generalTimeSup_H

#include "epicsTime.h"
#include "epicsTimer.h"
#include "libComAPI.h"

#define LAST_RESORT_PRIORITY 999

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*TIMECURRENTFUN)(epicsTimeStamp *pDest);
typedef int (*TIMEEVENTFUN)(epicsTimeStamp *pDest, int event);

LIBCOM_API int generalTimeRegisterCurrentProvider(const char *name,
    int priority, TIMECURRENTFUN getTime);
LIBCOM_API int generalTimeRegisterEventProvider(const char *name,
    int priority, TIMEEVENTFUN getEvent);

/* Original names, for compatibility */
#define generalTimeCurrentTpRegister generalTimeRegisterCurrentProvider
#define generalTimeEventTpRegister generalTimeRegisterEventProvider

LIBCOM_API int generalTimeAddIntCurrentProvider(const char *name,
    int priority, TIMECURRENTFUN getTime);
LIBCOM_API int generalTimeAddIntEventProvider(const char *name,
    int priority, TIMEEVENTFUN getEvent);

LIBCOM_API int generalTimeGetExceptPriority(epicsTimeStamp *pDest,
    int *pPrio, int ignorePrio);

#ifdef __cplusplus
}
#endif

#endif /* INC_generalTimeSup_H */
