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

#include <epicsTime.h>
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
union native_value {
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

/* External data reference.
 * If dtor is provided then it should be called when the referenced
 * data is no longer needed.  This is done automatically by
 * db_delete_field_log().  Any code which changes a dbfl_type_ref
 * field log to another type, or to reference different data,
 * must explicitly call the dtor function.
 */
struct dbfl_ref {
    dbfl_freeFunc     *dtor;  /* Callback to free filter-allocated resources */
    void              *pvt;   /* Private pointer */
    void              *field; /* Field value */
};

typedef struct db_field_log {
    unsigned int     type:2;  /* type (union) selector */
    /* ctx is used for all types */
    unsigned int      ctx:1;  /* context (operation type) */
    /* the following are used for value and reference types */
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

/*
 * A db_field_log will in one of three types:
 *
 * dbfl_type_rec - Reference to record
 *  The field log stores no data itself.  Data must instead be taken
 *  via the dbChannel* which must always be provided when along
 *  with the field log.
 *  For this type only the 'type' and 'ctx' members are used.
 *
 * dbfl_type_ref - Reference to outside value
 *  Used for variable size (array) data types.  Meta-data
 *  is stored in the field log, but value data is stored externally
 *  (see struct dbfl_ref).
 *  For this type all meta-data members are used.  The dbfl_ref side of the
 *  data union is used.
 *
 * dbfl_type_val - Internal value
 *  Used to store small scalar data.  Meta-data and value are
 *  present in this structure and no external references are used.
 *  For this type all meta-data members are used.  The dbfl_val side of the
 *  data union is used.
 */

#ifdef __cplusplus
}
#endif

#endif /*INCLdb_field_logh*/
