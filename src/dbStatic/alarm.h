/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Alarm definitions (Must Match choiceGbl.ascii) */
/* $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
 */

#ifndef INCalarmh
#define INCalarmh 1

#include "shareLib.h"
#include "epicsTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* defines for the choice fields */
/* ALARM SEVERITIES - NOTE: must match defs in choiceGbl.ascii GBL_ALARM_SEV */
#define NO_ALARM		0x0
#define	MINOR_ALARM		0x1
#define	MAJOR_ALARM		0x2
#define INVALID_ALARM		0x3
#define ALARM_NSEV		INVALID_ALARM+1

#ifndef NO_ALARMH_ENUM

typedef enum {
	epicsSevNone = NO_ALARM, 
	epicsSevMinor = MINOR_ALARM, 
	epicsSevMajor = MAJOR_ALARM, 
	epicsSevInvalid = INVALID_ALARM 
}epicsAlarmSeverity;
#define firstEpicsAlarmSev epicsSevNone
#define lastEpicsAlarmSev epicsSevInvalid 

#ifdef epicsAlarmGLOBAL
READONLY char *epicsAlarmSeverityStrings [lastEpicsAlarmSev+1] = {
		stringOf (epicsSevNone),
		stringOf (epicsSevMinor),
		stringOf (epicsSevMajor),
		stringOf (epicsSevInvalid),
};
#else /*epicsAlarmGLOBAL*/
epicsShareExtern READONLY char *epicsAlarmSeverityStrings [lastEpicsAlarmSev+1];
#endif /*epicsAlarmGLOBAL*/

#endif /* NO_ALARMH_ENUM */

/* ALARM STATUS  -NOTE: must match defs in choiceGbl.ascii GBL_ALARM_STAT */
/* NO_ALARM = 0 as above */
#define	READ_ALARM		1
#define	WRITE_ALARM		2
/* ANALOG ALARMS */
#define	HIHI_ALARM		3
#define	HIGH_ALARM		4
#define	LOLO_ALARM		5
#define	LOW_ALARM		6
/* BINARY ALARMS */
#define	STATE_ALARM		7
#define	COS_ALARM		8
/* other alarms */
#define COMM_ALARM		9
#define	TIMEOUT_ALARM		10
#define	HW_LIMIT_ALARM		11
#define	CALC_ALARM		12
#define	SCAN_ALARM		13
#define	LINK_ALARM		14
#define	SOFT_ALARM		15
#define	BAD_SUB_ALARM		16
#define	UDF_ALARM		17
#define	DISABLE_ALARM		18
#define	SIMM_ALARM		19
#define	READ_ACCESS_ALARM	20
#define	WRITE_ACCESS_ALARM	21
#define ALARM_NSTATUS		WRITE_ACCESS_ALARM + 1

#ifndef NO_ALARMH_ENUM

typedef enum {
	epicsAlarmNone = NO_ALARM,
	epicsAlarmRead = READ_ALARM, 
	epicsAlarmWrite = WRITE_ALARM, 
	epicsAlarmHiHi = HIHI_ALARM,
	epicsAlarmHigh = HIGH_ALARM,
	epicsAlarmLoLo = LOLO_ALARM,
	epicsAlarmLow = LOW_ALARM,
	epicsAlarmState = STATE_ALARM,
	epicsAlarmCos = COS_ALARM,
	epicsAlarmComm = COMM_ALARM,
	epicsAlarmTimeout = TIMEOUT_ALARM,
	epicsAlarmHwLimit = HW_LIMIT_ALARM,
	epicsAlarmCalc = CALC_ALARM,
	epicsAlarmScan = SCAN_ALARM,
	epicsAlarmLink = LINK_ALARM,
	epicsAlarmSoft = SOFT_ALARM,
	epicsAlarmBadSub = BAD_SUB_ALARM,
	epicsAlarmUDF = UDF_ALARM,
	epicsAlarmDisable = DISABLE_ALARM,
	epicsAlarmSimm = SIMM_ALARM,
	epicsAlarmReadAccess = READ_ACCESS_ALARM,
	epicsAlarmWriteAccess = WRITE_ACCESS_ALARM
}epicsAlarmCondition;
#define firstEpicsAlarmCond epicsSevNone
#define lastEpicsAlarmCond epicsAlarmWriteAccess 

#ifdef epicsAlarmGLOBAL
READONLY char *epicsAlarmConditionStrings [lastEpicsAlarmCond+1] = {
		stringOf (epicsAlarmNone),
		stringOf (epicsAlarmRead),
		stringOf (epicsAlarmWrite),
		stringOf (epicsAlarmHiHi),
		stringOf (epicsAlarmHigh),
		stringOf (epicsAlarmLoLo),
		stringOf (epicsAlarmLow),
		stringOf (epicsAlarmState),
		stringOf (epicsAlarmCos),
		stringOf (epicsAlarmComm),
		stringOf (epicsAlarmTimeout),
		stringOf (epicsAlarmHwLimit),
		stringOf (epicsAlarmCalc),
		stringOf (epicsAlarmScan),
		stringOf (epicsAlarmLink),
		stringOf (epicsAlarmSoft),
		stringOf (epicsAlarmBadSub),
		stringOf (epicsAlarmUDF),
		stringOf (epicsAlarmDisable),
		stringOf (epicsAlarmSimm),
		stringOf (epicsAlarmReadAccess),
		stringOf (epicsAlarmWriteAccess),
};
#else /*epicsAlarmGLOBAL*/
epicsShareExtern READONLY char *epicsAlarmConditionStrings [lastEpicsAlarmCond+1];
#endif /*epicsAlarmGLOBAL*/

#endif /* NO_ALARMH_ENUM */

#ifdef __cplusplus
}
#endif

#endif /* INCalarmh */

