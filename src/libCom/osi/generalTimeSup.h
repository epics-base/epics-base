/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2008 Diamond Light Source Ltd
* Copyright (c) 2004 Oak Ridge National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_generalTimeSup_H
#define INC_generalTimeSup_H

#include "epicsTime.h"
#include "epicsTimer.h"
#include "shareLib.h"

#define LAST_RESORT_PRIORITY 999

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*TIMECURRENTFUN)(epicsTimeStamp *pDest);
typedef int (*TIMEEVENTFUN)(epicsTimeStamp *pDest, int event);

epicsShareFunc int generalTimeCurrentTpRegister(const char *name, int priority,
    TIMECURRENTFUN getCurrent);
epicsShareFunc int generalTimeEventTpRegister(const char *name, int priority,
    TIMEEVENTFUN getEvent);
epicsShareFunc int generalTimeGetExceptPriority(epicsTimeStamp *pDest,
    int *pPriority, int ignore_priority);
epicsShareFunc epicsTimerId generalTimeCreateSyncTimer(epicsTimerCallback cb,
    void *param);

#ifdef __cplusplus
}
#endif

#endif /* INC_generalTimeSup_H */
