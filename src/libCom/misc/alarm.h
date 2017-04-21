/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Alarm definitions, must match menuAlarmSevr.dbd and menuAlarmStat.dbd */

/*
 *      Authors: Bob Dalesio and Marty Kraimer
 *      Date:    11-7-90
 */

#ifndef INC_alarm_H
#define INC_alarm_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif


#define NO_ALARM            0

/* ALARM SEVERITIES - must match menuAlarmSevr.dbd */

typedef enum {
    epicsSevNone = NO_ALARM,
    epicsSevMinor,
    epicsSevMajor,
    epicsSevInvalid,
    ALARM_NSEV
} epicsAlarmSeverity;

#define firstEpicsAlarmSev  epicsSevNone
#define MINOR_ALARM         epicsSevMinor
#define MAJOR_ALARM         epicsSevMajor
#define INVALID_ALARM       epicsSevInvalid
#define lastEpicsAlarmSev   epicsSevInvalid


/* ALARM STATUS - must match menuAlarmStat.dbd */

typedef enum {
    epicsAlarmNone = NO_ALARM,
    epicsAlarmRead,
    epicsAlarmWrite,
    epicsAlarmHiHi,
    epicsAlarmHigh,
    epicsAlarmLoLo,
    epicsAlarmLow,
    epicsAlarmState,
    epicsAlarmCos,
    epicsAlarmComm,
    epicsAlarmTimeout,
    epicsAlarmHwLimit,
    epicsAlarmCalc,
    epicsAlarmScan,
    epicsAlarmLink,
    epicsAlarmSoft,
    epicsAlarmBadSub,
    epicsAlarmUDF,
    epicsAlarmDisable,
    epicsAlarmSimm,
    epicsAlarmReadAccess,
    epicsAlarmWriteAccess,
    ALARM_NSTATUS
} epicsAlarmCondition;

#define firstEpicsAlarmCond epicsAlarmNone
#define READ_ALARM          epicsAlarmRead
#define WRITE_ALARM         epicsAlarmWrite
#define HIHI_ALARM          epicsAlarmHiHi
#define HIGH_ALARM          epicsAlarmHigh
#define LOLO_ALARM          epicsAlarmLoLo
#define LOW_ALARM           epicsAlarmLow
#define STATE_ALARM         epicsAlarmState
#define COS_ALARM           epicsAlarmCos
#define COMM_ALARM          epicsAlarmComm
#define TIMEOUT_ALARM       epicsAlarmTimeout
#define HW_LIMIT_ALARM      epicsAlarmHwLimit
#define CALC_ALARM          epicsAlarmCalc
#define SCAN_ALARM          epicsAlarmScan
#define LINK_ALARM          epicsAlarmLink
#define SOFT_ALARM          epicsAlarmSoft
#define BAD_SUB_ALARM       epicsAlarmBadSub
#define UDF_ALARM           epicsAlarmUDF
#define DISABLE_ALARM       epicsAlarmDisable
#define SIMM_ALARM          epicsAlarmSimm
#define READ_ACCESS_ALARM   epicsAlarmReadAccess
#define WRITE_ACCESS_ALARM  epicsAlarmWriteAccess
#define lastEpicsAlarmCond  epicsAlarmWriteAccess


/* Name string arrays */

epicsShareExtern const char *epicsAlarmSeverityStrings [ALARM_NSEV];
epicsShareExtern const char *epicsAlarmConditionStrings [ALARM_NSTATUS];


#ifdef __cplusplus
}
#endif

#endif /* INC_alarm_H */
