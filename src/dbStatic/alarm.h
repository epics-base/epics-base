/* Alarm definitions (Must Match choiceGbl.ascii) */
/* $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .00  mm-dd-yy        iii     Comment
 * .01  07-16-92        jba     changed VALID_ALARM to INVALID_ALARM
 * .02  08-11-92        jba     added new status DISABLE_ALARM, SIMM_ALARM
 * .03  05-11-94        jba     added new status READ_ACCESS_ALARM, WRITE_ACCESS_ALARM
 * $Log$
 * Revision 1.3  1998/03/12 20:43:35  jhill
 * fixed string defs
 *
 * Revision 1.2  1996/06/19 19:59:31  jhill
 * added missing defines/enums, corrected defines
 *
 *
 */

#ifndef INCalarmh
#define INCalarmh 1

#include "shareLib.h"
#include "epicsTypes.h"

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

#endif /* INCalarmh */

