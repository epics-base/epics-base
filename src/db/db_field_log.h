/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * 	Author: 	Jeffrey O. Hill 
 *      Date:            4-1-89
 */
#ifndef INCLdb_field_logh
#define INCLdb_field_logh

#include <epicsTypes.h>

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
 * Strings can be added to the set of types for which updates will be queued
 * by defining the macro DB_EVENT_LOG_STRINGS. The code in db_add_event()
 * will adjust automatically, it just compares field sizes.
 */
union native_value{
    epicsInt8		dbf_char;
    epicsInt16		dbf_short;
    epicsEnum16		dbf_enum;
    epicsInt32		dbf_long;
    epicsFloat32	dbf_float;
    epicsFloat64	dbf_double;
#ifdef DB_EVENT_LOG_STRINGS
    char		dbf_string[MAX_STRING_SIZE];
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
