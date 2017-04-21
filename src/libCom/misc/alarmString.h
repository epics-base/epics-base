/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * This file is deprecated, use alarm.h instead.
 *
 * Old string names for alarm status and severity values
 */

#ifndef INC_alarmString_H
#define INC_alarmString_H

#include "alarm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Old versions of alarmString.h defined these names: */

#define alarmSeverityString epicsAlarmSeverityStrings
#define alarmStatusString epicsAlarmConditionStrings


#ifdef __cplusplus
}
#endif

#endif
