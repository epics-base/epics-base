/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Jeffrey O. Hill <johill@lanl.gov>
 *
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#ifndef INCLdb_field_logh
#define INCLdb_field_logh

#include "epicsTime.h"

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
union native_value {
      	short		dbf_int;
      	short		dbf_short;
      	float		dbf_float;
      	short		dbf_enum;
      	char		dbf_char;
      	long		dbf_long;
      	double		dbf_double;
#ifdef DB_EVENT_LOG_STRINGS
        char		dbf_string[MAX_STRING_SIZE];
#endif
};

/*
 *	structure to log the state of a data base field at the time
 *	an event is triggered.
 */
struct db_field_log;
typedef void (dbfl_freeFunc)(struct db_field_log *pfl);

/* Types of db_field_log: rec = use record, val = val inside, ref = reference inside */
typedef enum dbfl_type {
    dbfl_type_rec = 0,
    dbfl_type_val,
    dbfl_type_ref
} dbfl_type;

/* Context of db_field_log: event = subscription update, read = read reply */
typedef enum dbfl_context {
    dbfl_context_read = 0,
    dbfl_context_event
} dbfl_context;

#define dbflTypeStr(t) (t==dbfl_type_val?"val":t==dbfl_type_rec?"rec":"ref")

struct dbfl_val {
    union native_value field; /* Field value */
};

struct dbfl_ref {
    dbfl_freeFunc     *dtor;  /* Callback to free filter-allocated resources */
    void              *pvt;   /* Private pointer */
    void              *field; /* Field value */
};

typedef struct db_field_log {
    enum dbfl_type   type:2;  /* type (union) selector */
    enum dbfl_context ctx:1;  /* context (operation type) */
    epicsTimeStamp     time;  /* Time stamp */
    unsigned short     stat;  /* Alarm Status */
    unsigned short     sevr;  /* Alarm Severity */
    short        field_type;  /* DBF type of data */
    short        field_size;  /* Data size */
    long        no_elements;  /* No of array elements */
    union {
        struct dbfl_val v;
        struct dbfl_ref r;
    } u;
} db_field_log;

#ifdef __cplusplus
}
#endif

#endif /*INCLdb_field_logh*/
