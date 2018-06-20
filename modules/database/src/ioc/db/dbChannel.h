/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#ifndef INC_dbChannel_H
#define INC_dbChannel_H

#include "dbDefs.h"
#include "dbAddr.h"
#include "ellLib.h"
#include "epicsTypes.h"
#include "errMdef.h"
#include "shareLib.h"
#include "db_field_log.h"
#include "dbEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * event subscription
 */
typedef struct evSubscrip {
    ELLNODE                 node;
    struct dbChannel        *chan;
    EVENTFUNC               *user_sub;
    void                    *user_arg;
    struct event_que        *ev_que;
    db_field_log            **pLastLog;
    unsigned long           npend;  /* n times this event is on the queue */
    unsigned long           nreplace;  /* n times replacing event on the queue */
    unsigned char           select;
    char                    useValque;
    char                    callBackInProgress;
    char                    enabled;
} evSubscrip;

typedef struct chFilter chFilter;

/* A dbChannel points to a record field, and can have multiple filters */
typedef struct dbChannel {
    const char *name;
    dbAddr addr;              /* address structure for record/field */
    long  final_no_elements;  /* final number of elements (arrays) */
    short final_field_size;   /* final size of element */
    short final_type;         /* final type of database field */
    ELLLIST filters;          /* list of filters as created from JSON */
    ELLLIST pre_chain;        /* list of filters to be called pre-event-queue */
    ELLLIST post_chain;       /* list of filters to be called post-event-queue */
} dbChannel;

/* Prototype for the channel event function that is called in filter stacks
 *
 * When invoked the scan lock for the record associated with 'chan' _may_ be locked.
 * If pLog->type==dbfl_type_rec then dbScanLock() must be called before copying
 * data out of the associated record.
 *
 * This function has ownership of the field log pLog, if it wishes to discard
 * this update it should free the field log with db_delete_field_log() and
 * then return NULL.
 */
typedef db_field_log* (chPostEventFunc)(void *pvt, dbChannel *chan, db_field_log *pLog);

/* Return values from chFilterIf->parse_* routines: */
typedef enum {
    parse_stop, parse_continue
} parse_result;

/* These routines must be implemented by each filter plug-in */
typedef struct chFilterIf {
    /* cleanup pointer passed to dbRegisterFilter().
     * Called during DB shutdown
     */
    void (* priv_free)(void *puser);
    /* Parsing event handlers: */
    parse_result (* parse_start)(chFilter *filter);
    /* If parse_start() returns parse_continue for a filter, one of
     * parse_abort() or parse_end() will later be called for that same
     * filter.
     */
    void (* parse_abort)(chFilter *filter);
    /* If parse_abort() is called it should release any memory allocated
     * for this filter; no further parse_...() calls will be made;
     */
    parse_result (* parse_end)(chFilter *filter);
    /* If parse_end() returns parse_stop it should have released any
     * memory allocated for this filter; no further parse_...() calls will
     * be made in this case.
     */

    parse_result (* parse_null)(chFilter *filter);
    parse_result (* parse_boolean)(chFilter *filter, int boolVal);
    parse_result (* parse_integer)(chFilter *filter, long integerVal);
    parse_result (* parse_double)(chFilter *filter, double doubleVal);
    parse_result (* parse_string)(chFilter *filter, const char *stringVal,
            size_t stringLen); /* NB: stringVal is not zero-terminated: */

    parse_result (* parse_start_map)(chFilter *filter);
    parse_result (* parse_map_key)(chFilter *filter, const char *key,
            size_t stringLen); /* NB: key is not zero-terminated: */
    parse_result (* parse_end_map)(chFilter *filter);

    parse_result (* parse_start_array)(chFilter *filter);
    parse_result (* parse_end_array)(chFilter *filter);

    /* Channel operations: */
    long (* channel_open)(chFilter *filter);
    void (* channel_register_pre) (chFilter *filter, chPostEventFunc **cb_out, void **arg_out, db_field_log *probe);
    void (* channel_register_post)(chFilter *filter, chPostEventFunc **cb_out, void **arg_out, db_field_log *probe);
    void (* channel_report)(chFilter *filter, int level, const unsigned short indent);
    void (* channel_close)(chFilter *filter);
} chFilterIf;

/* A chFilterPlugin holds data for a filter plugin */
typedef struct chFilterPlugin {
    ELLNODE node;
    const char *name;
    const chFilterIf *fif;
    void *puser;
} chFilterPlugin;

/* A chFilter holds data for a single filter instance */
struct chFilter {
    ELLNODE list_node;
    ELLNODE pre_node;
    ELLNODE post_node;
    dbChannel *chan;
    const chFilterPlugin *plug;
    chPostEventFunc *pre_func;
    void *pre_arg;
    chPostEventFunc *post_func;
    void *post_arg;
    void *puser;
};

struct dbCommon;
struct dbFldDes;

epicsShareFunc void dbChannelInit (void);
epicsShareFunc void dbChannelExit(void);
epicsShareFunc long dbChannelTest(const char *name);
epicsShareFunc dbChannel * dbChannelCreate(const char *name);
epicsShareFunc long dbChannelOpen(dbChannel *chan);

/*Following is also defined in db_convert.h*/
epicsShareExtern unsigned short dbDBRnewToDBRold[];

/* In the following macros pChan is dbChannel* */

/* evaluates to const char* */
#define dbChannelName(pChan) ((pChan)->name)

/* evaluates to struct dbCommon* */
#define dbChannelRecord(pChan) ((pChan)->addr.precord)

/* evaluates to struct dbFldDes* */
#define dbChannelFldDes(pChan) ((pChan)->addr.pfldDes)

/* evaluates to long */
#define dbChannelElements(pChan) ((pChan)->addr.no_elements)

/* evaluates to short */
#define dbChannelFieldType(pChan) ((pChan)->addr.field_type)

/* evaluates to short */
#define dbChannelExportType(pChan) ((pChan)->addr.dbr_field_type)

/* evaluates to short */
#define dbChannelExportCAType(pChan) (dbDBRnewToDBRold[dbChannelExportType(pChan)])

/* evaluates to short */
#define dbChannelFieldSize(pChan) ((pChan)->addr.field_size)

/* evaluates to long */
#define dbChannelFinalElements(pChan) ((pChan)->final_no_elements)

/* evaluates to short */
#define dbChannelFinalFieldType(pChan) ((pChan)->final_type)

/* evaluates to short */
#define dbChannelFinalCAType(pChan) (dbDBRnewToDBRold[(pChan)->final_type])

/* evaluates to short */
#define dbChannelFinalFieldSize(pChan) ((pChan)->final_field_size)

/* evaluates to short */
#define dbChannelSpecial(pChan) ((pChan)->addr.special)

/* Channel filters do not get to interpose here since there are many
 * places where the field pointer is compared with the address of a
 * specific record field, so they can't modify the pointer value.
 */
/* evaluates to void* */
#define dbChannelField(pChan) ((pChan)->addr.pfield)


epicsShareFunc long dbChannelGet(dbChannel *chan, short type,
        void *pbuffer, long *options, long *nRequest, void *pfl);
epicsShareFunc long dbChannelGetField(dbChannel *chan, short type,
        void *pbuffer, long *options, long *nRequest, void *pfl);
epicsShareFunc long dbChannelPut(dbChannel *chan, short type,
        const void *pbuffer, long nRequest);
epicsShareFunc long dbChannelPutField(dbChannel *chan, short type,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbChannelShow(dbChannel *chan, int level,
        const unsigned short indent);
epicsShareFunc void dbChannelFilterShow(dbChannel *chan, int level,
        const unsigned short indent);
epicsShareFunc void dbChannelDelete(dbChannel *chan);

epicsShareFunc void dbRegisterFilter(const char *key, const chFilterIf *fif, void *puser);
epicsShareFunc db_field_log* dbChannelRunPreChain(dbChannel *chan, db_field_log *pLogIn);
epicsShareFunc db_field_log* dbChannelRunPostChain(dbChannel *chan, db_field_log *pLogIn);
epicsShareFunc const chFilterPlugin * dbFindFilter(const char *key, size_t len);
epicsShareFunc void dbChannelMakeArrayCopy(void *pvt, db_field_log *pfl, dbChannel *chan);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbChannel_H */
