/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  $Id$ */

/* String values for alarms (compatibility header) */

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

/* Use the strings from alarm.h */

#define alarmSeverityString epicsAlarmSeverityStrings
epicsShareExtern const char *epicsAlarmSeverityStrings [];

#define alarmStatusString epicsAlarmConditionStrings
epicsShareExtern const char *epicsAlarmConditionStrings [];

#ifdef __cplusplus
}
#endif

#endif
