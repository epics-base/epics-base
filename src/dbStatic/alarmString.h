/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  $Id$ */

/* alarmString.h - String values for alarms (Must match alarm.h)!!!!*/
/* share/epicsH @(#)alarmString.h	1.3     11/6/90 */

/* As of January 2004 (EPICS Base 3.14.5), the use of this file is
 *
 *                      D E P R E C A T E D
 *
 *         Please use the definitions in alarm.h instead.           */


#ifndef INCalarmStringh
#define INCalarmStringh 1

#ifdef __cplusplus
extern "C" {
#endif

const char * alarmSeverityString[]={
	"NO_ALARM",
	"MINOR",
	"MAJOR",
	"INVALID"
	};


/*** note:  this should be reconciled with alarm.h ***/
const char * alarmStatusString[]={
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
	"WRITE_ACCESS"};

#ifdef __cplusplus
}
#endif

#endif
