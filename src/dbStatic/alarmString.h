/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* String names for alarms */

#ifndef INC_alarmString_H
#define INC_alarmString_H

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility with original alarmString.h names */

#define alarmSeverityString epicsAlarmSeverityStrings
#define alarmStatusString epicsAlarmConditionStrings


/* Name strings */

/* ALARM SEVERITIES - must match menuAlarmSevr.dbd and alarm.h */

const char * epicsAlarmSeverityStrings[] = {
    "NO_ALARM",
    "MINOR",
    "MAJOR",
    "INVALID"
};


/* ALARM STATUS - must match menuAlarmStat.dbd and alarm.h */

const char * epicsAlarmConditionStrings[] = {
    "NO_ALARM",
    "READ",
    "WRITE",
    "HIHI",
    "HIGH",
    "LOLO",
    "LOW",
    "STATE",
    "COS",
    "COMM",
    "TIMEOUT",
    "HWLIMIT",
    "CALC",
    "SCAN",
    "LINK",
    "SOFT",
    "BAD_SUB",
    "UDF",
    "DISABLE",
    "SIMM",
    "READ_ACCESS",
    "WRITE_ACCESS"
};

#ifdef __cplusplus
}
#endif

#endif
