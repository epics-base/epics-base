/*  $Id$
 *      Author: 
 *      Date:   
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
 * .01  06-06-91        bkc     Modified alarmSeverityString: add VALID alarm,
 *				remove INFO alarm
 * .02  09-10-91        bkc     Change status string corresponding to 
 *				dbconV2/menus.c
 * .03  07-16-92        jba     changed VALID_ALARM to INVALID_ALARM
 * .04  02-03-93        jba     added 2 new status values
 * .05  05-11-94        jba     addd READ_ACCESS, WRITE_ACCESS, rmvd EPICS_V2 ifdef
 *      ...
 */

/* alarmString.h - String values for alarms (Must match alarm.h)!!!!*/
/* share/epicsH @(#)alarmString.h	1.3     11/6/90 */
#ifndef INCalarmStringh
#define INCalarmStringh 1


static char *alarmStringhSccsId = "$Id$";

char * alarmSeverityString[]={
	"NO_ALARM",
	"MINOR",
	"MAJOR",
	"INVALID"
	};


/*** note:  this should be reconciled with alarm.h ***/


char * alarmStatusString[]={
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

#endif
