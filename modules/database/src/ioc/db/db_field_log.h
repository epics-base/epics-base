/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
    epicsInt8 dbf_char;
    epicsUInt8 dbf_uchar;
    epicsInt16 dbf_short;
    epicsUInt16 dbf_ushort;
    epicsEnum16 dbf_enum;
    epicsInt32 dbf_long;
    epicsUInt32 dbf_ulong;
    epicsInt64 dbf_int64;
    epicsUInt64 dbf_uint64;
    epicsFloat32 dbf_float;
    epicsFloat64 dbf_double;
#ifdef DB_EVENT_LOG_STRINGS
    char            dbf_string[MAX_STRING_SIZE];
#endif
};

/*
 *  structure to log the state of a data base field at the time
 *  an event is triggered.
 */
struct db_field_log;
typedef void (dbfl_freeFunc)(struct db_field_log *pfl);

/*
 * A db_field_log has one of two types:
 *
 * dbfl_type_ref - Reference to value
 *  Used for variable size (array) data types.  Meta-data
 *  is stored in the field log, but value data is stored externally.
 *  Only the dbfl_ref side of the data union is valid.
 *
 * dbfl_type_val - Internal value
 *  Used to store small scalar data.  Meta-data and value are
 *  present in this structure and no external references are used.
 *  Only the dbfl_val side of the data union is valid.
 */
typedef enum dbfl_type {
    dbfl_type_val,
    dbfl_type_ref
} dbfl_type;

/* Context of db_field_log: event = subscription update, read = read reply */
typedef enum dbfl_context {
    dbfl_context_read,
    dbfl_context_event
} dbfl_context;

#define dbflTypeStr(t) (t==dbfl_type_val?"val":"ref")

struct dbfl_val {
    union native_value field; /* Field value */
};

/* External data reference.
 * If dtor is provided then it should be called when the referenced
 * data is no longer needed.  This is done automatically by
 * db_delete_field_log().  Any code which changes a dbfl_type_ref
 * field log to another type, or to reference different data,
 * must explicitly call the dtor function.
 * If the dtor is NULL and no_elements > 0, then this means the array
 * data is still owned by a record. See the macro dbfl_has_copy below.
 */
struct dbfl_ref {
    void              *pvt;   /* Private pointer */
    void              *field; /* Field value */
};

/*
 * Note that field_size may be larger than MAX_STRING_SIZE.
 */
typedef struct db_field_log {
    unsigned int     type:1;  /* type (union) selector */
    /* ctx is used for all types */
    unsigned int      ctx:1;  /* context (operation type) */
    /* only for dbfl_context_event */
    unsigned char      mask;  /* DBE_* mask */
    /* the following are used for value and reference types */
    epicsTimeStamp     time;  /* Time stamp */
    epicsUTag          utag;
    unsigned short     stat;  /* Alarm Status */
    unsigned short     sevr;  /* Alarm Severity */
    char               amsg[40];
    short        field_type;  /* DBF type of data */
    short        field_size;  /* Size of a single element */
    long        no_elements;  /* No of valid array elements */
    dbfl_freeFunc     *dtor;  /* Callback to free filter-allocated resources */
    union {
        struct dbfl_val v;
        struct dbfl_ref r;
    } u;
} db_field_log;

/*
 * Whether a db_field_log* owns the field data. If this is the case, then the
 * db_field_log is fully decoupled from the record: there is no need to lock
 * the record when accessing the data, nor to call any rset methods (like
 * get_array_info) because this has already been done when we made the copy. A
 * special case here is that of no (remaining) data (i.e. no_elements==0). In
 * this case, making a copy is redundant, so we have no dtor. But conceptually
 * the db_field_log still owns the (empty) data.
 */
#define dbfl_has_copy(p)\
 ((p) && ((p)->type==dbfl_type_val || (p)->dtor || (p)->no_elements==0))

#define dbfl_pfield(p)\
 ((p)->type==dbfl_type_val ? &p->u.v.field : p->u.r.field)

#ifdef __cplusplus
}
#endif

#endif /*INCLdb_field_logh*/
