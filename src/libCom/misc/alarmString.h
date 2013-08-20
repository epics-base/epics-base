/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$ */

/* String names for alarm status and severity values */

#ifndef INC_alarmString_H
#define INC_alarmString_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An older version of alarmString.h used these names: */

#define alarmSeverityString epicsAlarmSeverityStrings
#define alarmStatusString epicsAlarmConditionStrings


/* Name string arrays */

epicsShareExtern const char *epicsAlarmSeverityStrings [ALARM_NSEV];
epicsShareExtern const char *epicsAlarmConditionStrings [ALARM_NSTATUS];


#ifdef __cplusplus
}
#endif

#endif
