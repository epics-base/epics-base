/* $Id$
 * 	Author: 	Jeffrey O. Hill 
 *      Date:            4-1-89
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
 *	NOTES:
 *
 * Modification Log:
 * -----------------
 */
#ifndef INCLdb_field_logh
#define INCLdb_field_logh

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
typedef struct{
        unsigned short		stat;	/* Alarm Status         */
        unsigned short		sevr;	/* Alarm Severity       */
	TS_STAMP		time;	/* time stamp		*/
	union native_value	field;	/* field value		*/
}db_field_log;

#endif /*INCLdb_field_logh*/
