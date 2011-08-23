/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$
 * 	Author: 	Jeffrey O. Hill 
 *      Date:            4-1-89
 */
#ifndef INCLdb_field_logh
#define INCLdb_field_logh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple native types (anything which is not a string or an array for
 * now) logged by db_post_events() for reliable interprocess communication.
 * (for other types they get whatever happens to be there when the lower
 * priority task pending on the event queue wakes up). Strings would slow down
 * events for more reasonable size values. DB fields of native type string
 * will most likely change infrequently.
 * 
 */
union native_value{
      	short		dbf_int;
      	short		dbf_short;
      	float		dbf_float;
      	short		dbf_enum;
      	char		dbf_char;
      	long		dbf_long;
      	double		dbf_double;
#ifdef DB_EVENT_LOG_STRINGS
      	char		dbf_string[MAXSTRINGSIZE];
#endif
};

/*
 *	structure to log the state of a data base field at the time
 *	an event is triggered.
 */
typedef struct db_field_log {
        unsigned short		stat;	/* Alarm Status         */
        unsigned short		sevr;	/* Alarm Severity       */
	epicsTimeStamp		time;	/* time stamp		*/
	union native_value	field;	/* field value		*/
}db_field_log;

#ifdef __cplusplus
}
#endif

#endif /*INCLdb_field_logh*/
