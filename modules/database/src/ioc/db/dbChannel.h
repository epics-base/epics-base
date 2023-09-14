/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbChannel_H
#define INC_dbChannel_H

/** \file dbChannel.h
 *
 * \author Andrew Johnson (ANL)
 * \author Ralph Lange (BESSY)
 *
 * \brief The dbChannel API gives access to record fields.
 *
 * The dbChannel API is used internally by the IOC and by link types, device
 * support and IOC servers (RSRV and QSRV) to access record fields, either
 * directly or through one or more server-side filters as specified in the
 * channel name used when creating the channel.
 */

#include "dbDefs.h"
#include "dbAddr.h"
#include "ellLib.h"
#include "epicsTypes.h"
#include "errMdef.h"
#include "db_field_log.h"
#include "dbEvent.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * event subscription
 */
struct evSubscrip;

typedef struct evSubscrip evSubscrip;

#ifdef EPICS_PRIVATE_API
struct evSubscrip {
    ELLNODE             node;
    struct dbChannel  * chan;
    /* user_sub==NULL used to indicate db_cancel_event() */
    EVENTFUNC         * user_sub;
    void              * user_arg;
    /* associated queue, may be shared with other evSubscrip */
    struct event_que  * ev_que;
    /* NULL if !npend.  if npend!=0, pointer to last event added to event_que::valque */
    db_field_log     ** pLastLog;
    /* n times this event is on the queue */
    unsigned long       npend;
    /* n times replacing event on the queue */
    unsigned long       nreplace;
    /* DBE mask */
    unsigned char       select;
    /* if set, subscription will yield dbfl_type_val */
    char                useValque;
    /* event_task is handling this subscription */
    char                callBackInProgress;
    /* this node added to dbCommon::mlis */
    char                enabled;
};
#endif

typedef struct chFilter chFilter;

/** \brief A Database Channel object
 *
 * A dbChannel is created from a user-supplied channel name, and holds
 * pointers to the record & field and information about any filters that
 * were specified with it. The dbChannel macros defined in this header
 * file should always be used to read data from a dbChannel object, the
 * internal implementation may change without notice.
 */
typedef struct dbChannel {
    const char *name;         /**< Channel name */
    dbAddr addr;              /**< Pointers to record & field */
    long  final_no_elements;  /**< Final number of array elements */
    short final_field_size;   /**< Final size of each element */
    short final_type;         /**< Final type of database field */
    ELLLIST filters;          /**< Filters used by dbChannel */
    ELLLIST pre_chain;        /**< Filters on pre-event-queue chain */
    ELLLIST post_chain;       /**< Filters on post-event-queue chain */
} dbChannel;

/** \brief Event filter function type
 *
 * Prototype for channel event filter functions.
 *
 * When these functions are called the scan lock for the record associated
 * with \p chan _may_ already be locked, but they must use dbfl_has_copy()
 * to determine whether the data in \p pLog belongs to the record. If that
 * returns 0 the function must call dbScanLock() before accessing the data.
 *
 * A filter function owns the field log \p pLog when called. To discard an
 * update it should free the field log using db_delete_field_log() and
 * return NULL.
 */
typedef db_field_log* (chPostEventFunc)(void *pvt, dbChannel *chan, db_field_log *pLog);

/** \brief Result returned by chFilterIf parse routines.
 *
 * The parsing functions from a chFilterIf must return either \p parse_stop
 * (in event of an error) or \p parse_continue.
 */
typedef enum {
    parse_stop, parse_continue
} parse_result;

/** \brief Channel Filter Interface
 *
 * Routines to be implemented by each Channel Filter.
 */
typedef struct chFilterIf {
    /** \brief Release private filter data.
     *
     * Called during database shutdown to release resources allocated by
     * the filter.
     * \param puser The user-pointer passed into dbRegisterFilter().
     */
    void (* priv_free)(void *puser);

    /** \name Parsing event handlers
     *
     * A filter that doesn't accept a particular JSON value type may use a
     * \p NULL pointer to the parsing handler for that value type, which is
     * equivalent to a routine that always returns \p parse_stop.
     */

    /** \brief Create new filter instance.
     *
     * Called when a new filter instance is requested. Filter may allocate
     * resources for this instance and store in \p filter->puser.
     * If parse_start() returns \p parse_continue for a filter, one of
     * parse_abort() or parse_end() will later be called for that same
     * filter.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_start)(chFilter *filter);

    /** \brief Parsing of filter instance is being cancelled.
     *
     * This function should release any memory allocated for the given
     * \p filter instance; no further parsing handlers will be called for it.
     * \param filter Pointer to instance data.
     */
    void (* parse_abort)(chFilter *filter);

    /** \brief Parsing of filter instance has completed successfully.
     *
     * The parser has reached the end of this instance and no further parsing
     * handlers will be called for it. The filter must check the instance
     * data and indicate whether it was complete or not.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_end)(chFilter *filter);

    /** \brief Parser saw \p null value.
     *
     * Optional.
     * Null values are rarely accepted by channel filters.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_null)(chFilter *filter);

    /** \brief Parser saw boolean value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param boolVal true/false Value.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_boolean)(chFilter *filter, int boolVal);

    /** \brief Parser saw integer value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param integerVal Value.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_integer)(chFilter *filter, long integerVal);

    /** \brief Parser saw double value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param doubleVal Value.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_double)(chFilter *filter, double doubleVal);

    /** \brief Parser saw string value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param stringVal Value, not zero-terminated.
     * \param stringLen Number of chars in \p stringVal.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_string)(chFilter *filter, const char *stringVal,
            size_t stringLen);

    /** \brief Parser saw start of a JSON map value.
     *
     * Optional.
     * Inside a JSON map all data consists of key/value pairs.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_start_map)(chFilter *filter);

    /** \brief Parser saw a JSON map key.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param key Value not zero-terminated.
     * \param stringLen Number of chars in \p key
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_map_key)(chFilter *filter, const char *key,
            size_t stringLen);

    /** \brief Parser saw end of a JSON map value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_end_map)(chFilter *filter);

    /** \brief Parser saw start of a JSON array value.
     *
     * Optional.
     * Data inside a JSON array doesn't have to be all of the same type.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_start_array)(chFilter *filter);

    /** \brief Parser saw end of a JSON array value.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \returns \p parse_stop on error, or \p parse_continue
     */
    parse_result (* parse_end_array)(chFilter *filter);

    /** \name Channel operations */

    /** \brief Open filter on channel.
     *
     * Optional, initialize instance.
     * \param filter Pointer to instance data.
     * \returns 0, or an error status value.
     */
    long (* channel_open)(chFilter *filter);

    /** \brief Get pre-chain filter function.
     *
     * Optional.
     * Returns pre-chain filter function and context.
     * \param[in] filter Pointer to instance data.
     * \param[out] cb_out Write filter function pointer here.
     * \param[out] arg_out Write private data pointer here.
     * \param[in,out] probe db_field_log with metadata for adjusting.
     */
    void (* channel_register_pre) (chFilter *filter,
        chPostEventFunc **cb_out, void **arg_out, db_field_log *probe);

    /** \brief Get post-chain filter function.
     *
     * Optional, return post-chain filter function and context.
     * \param[in] filter Pointer to instance data.
     * \param[out] cb_out Write filter function pointer here.
     * \param[out] arg_out Write private data pointer here.
     * \param[in,out] probe db_field_log with metadata for adjusting.
     */
    void (* channel_register_post)(chFilter *filter,
        chPostEventFunc **cb_out, void **arg_out, db_field_log *probe);

    /** \brief Print information about filter to stdout.
     *
     * Optional.
     * \param filter Pointer to instance data.
     * \param level Higher levels may provide more detail.
     * \param indent Indent all lines by this many spaces.
     */
    void (* channel_report)(chFilter *filter,
        int level, const unsigned short indent);

    /** \brief Close filter.
     *
     * Optional, releases resources allocated for this instance.
     * \param filter Pointer to instance data.
     */
    void (* channel_close)(chFilter *filter);
} chFilterIf;

/** \brief Filter plugin data
 * 
 * A chFilterPlugin object holds data about a filter plugin.
 */
typedef struct chFilterPlugin {
    ELLNODE node;           /**< \brief List node (dbBase->filterList) */
    const char *name;       /**< \brief Filter name */
    const chFilterIf *fif;  /**< \brief Filter interface routines */
    void *puser;            /**< \brief For use by the plugin */
} chFilterPlugin;

/** \brief Filter instance data
 *
 * A chFilter holds data about a single filter instance.
 */
struct chFilter {
    ELLNODE list_node;          /**< \brief List node (dbChannel->filters) */
    ELLNODE pre_node;           /**< \brief List node (dbChannel->pre_chain) */
    ELLNODE post_node;          /**< \brief List node (dbChannel->post_chain) */
    dbChannel *chan;            /**< \brief The dbChannel we belong to */
    const chFilterPlugin *plug; /**< \brief The plugin that created us */
    chPostEventFunc *pre_func;  /**< \brief pre-chain filter function */
    void *pre_arg;              /**< \brief pre-chain context pointer */
    chPostEventFunc *post_func; /**< \brief post-chain filter function */
    void *post_arg;             /**< \brief post-chain context pointer */
    void *puser;                /**< \brief For use by the plugin */
};

struct dbCommon;
struct dbFldDes;

/** \brief Initialize the dbChannel subsystem. */
DBCORE_API void dbChannelInit(void);

/** \brief Cleanup the dbChannel subsystem. */
DBCORE_API void dbChannelExit(void);

/** \brief Test the given PV name for existance.
 *
 * This routine looks up the given record and field name, but does not check
 * whether any field modifiers given after the field name are correct.
 * This is sufficient for the correct server to quickly direct searches to the
 * IOC that owns that PV name. Field modifiers will be checked when
 * dbChannelCreate() is later called with the same name.
 * \param name Channel name.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelTest(const char *name);

/** \brief Create a dbChannel object for the given PV name.
 *
 * \param name Channel name.
 * \return Pointer to dbChannel object, or NULL if invalid.
 */
DBCORE_API dbChannel * dbChannelCreate(const char *name);

/** \brief Open a dbChannel for doing I/O.
 *
 * \param chan Pointer to the dbChannel object.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelOpen(dbChannel *chan);

/** \brief Request (DBR) type conversion array.
 *
 * This converter array is declared in db_convert.h but redeclared
 * here as it is needed by the dbChannel...CAType macros defined here.
 */
DBCORE_API extern unsigned short dbDBRnewToDBRold[];

/** \name dbChannel Inspection Macros */

/** \brief Name that defined the channel.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns const char*
 */
#define dbChannelName(pChan) ((pChan)->name)

/** \brief Record the channel connects to.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns struct dbCommon*
 */
#define dbChannelRecord(pChan) ((pChan)->addr.precord)

/** \brief Field descriptor for the field pointed to.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns struct dbFldDes*
 */
#define dbChannelFldDes(pChan) ((pChan)->addr.pfldDes)

/** \brief Number of array elements in the field.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns long
 */
#define dbChannelElements(pChan) ((pChan)->addr.no_elements)

/** \brief Data type (DBF type) of the field.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelFieldType(pChan) ((pChan)->addr.field_type)

/** \brief Request type (DBR type) of the field.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelExportType(pChan) ((pChan)->addr.dbr_field_type)

/** \brief CA data type of the field.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelExportCAType(pChan) (dbDBRnewToDBRold[dbChannelExportType(pChan)])

/** \brief Field (element if array) size in bytes.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelFieldSize(pChan) ((pChan)->addr.field_size)

/** \brief Array length after filtering.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns long
 */
#define dbChannelFinalElements(pChan) ((pChan)->final_no_elements)

/** \brief Data type after filtering.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelFinalFieldType(pChan) ((pChan)->final_type)

/** \brief Channel CA data type after filtering.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelFinalCAType(pChan) (dbDBRnewToDBRold[(pChan)->final_type])

/** \brief Field/element size after filtering, in bytes.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short */
#define dbChannelFinalFieldSize(pChan) ((pChan)->final_field_size)

/** \brief Field special attribute.
 *
 * \param pChan Pointer to the dbChannel object.
 * \returns short
 */
#define dbChannelSpecial(pChan) ((pChan)->addr.special)

/** \brief Pointer to the record field.
 *
 * Channel filters do not get to interpose here since there are many
 * places where the field pointer is compared with the address of a
 * specific record field, so they can't modify the pointer value.
 * \param pChan Pointer to the dbChannel object.
 * \returns void *
 */
#define dbChannelField(pChan) ((pChan)->addr.pfield)


/** \name dbChannel Operation Functions */

/** \brief dbGet() through a dbChannel.
 *
 * Calls dbGet() for the field that \p chan refers to.
 * Only call this routine if the record is already locked.
 * \param[in] chan Pointer to the dbChannel object.
 * \param[in] type Request type from dbFldTypes.h.
 * \param[out] pbuffer Pointer to data buffer.
 * \param[in,out] options Request options from dbAccessDefs.h.
 * \param[in,out] nRequest Pointer to the element count.
 * \param[in] pfl Pointer to a db_field_log or NULL.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelGet(dbChannel *chan, short type,
        void *pbuffer, long *options, long *nRequest, void *pfl);

/** \brief dbGetField() through a dbChannel.
 *
 * Get values from a PV through a channel.
 * This routine locks the record, calls
 * dbChannelGet(), then unlocks the record again.
 * \param[in] chan Pointer to the dbChannel object.
 * \param[in] type Request type from dbFldTypes.h.
 * \param[out] pbuffer Pointer to data buffer.
 * \param[in,out] options Request options from dbAccessDefs.h.
 * \param[in,out] nRequest Pointer to the element count.
 * \param[in] pfl Pointer to a db_field_log or NULL.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelGetField(dbChannel *chan, short type,
        void *pbuffer, long *options, long *nRequest, void *pfl);

/** \brief dbPut() through a dbChannel.
 *
 * Put values to a PV through a channel. Only call this routine if the
 * record is already locked.
 * Calls dbPut() for the field that \p chan refers to.
 * \param chan[in] Pointer to the dbChannel object.
 * \param type[in] Request type from dbFldTypes.h.
 * \param pbuffer[in] Pointer to data buffer.
 * \param nRequest[in] Number of elements in pbuffer.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelPut(dbChannel *chan, short type,
        const void *pbuffer, long nRequest);

/** \brief dbPutField() through a dbChannel.
 *
 * Put values to a PV through a channel.
 * This routine calls dbPutField() for the field that \p chan refers to.
 * \param chan[in] Pointer to the dbChannel object.
 * \param type[in] Request type from dbFldTypes.h.
 * \param pbuffer[in] Pointer to data buffer.
 * \param nRequest[in] Number of elements in pbuffer.
 * \returns 0, or an error status value.
 */
DBCORE_API long dbChannelPutField(dbChannel *chan, short type,
        const void *pbuffer, long nRequest);

/** \brief Print report on a channel.
 *
 * Print information about the channel to stdout.
 * \param chan Pointer to the dbChannel object.
 * \param level Higher levels may provide more detail.
 * \param indent Indent all lines by this many spaces.
 */
DBCORE_API void dbChannelShow(dbChannel *chan, int level,
        const unsigned short indent);

/** \brief Print report on a channel's filters.
 *
 * Print information about the channel's filters to stdout.
 * \param chan Pointer to the dbChannel object.
 * \param level Higher levels may provide more detail.
 * \param indent Indent all lines by this many spaces.
 */
DBCORE_API void dbChannelFilterShow(dbChannel *chan, int level,
        const unsigned short indent);

/** \brief Delete a channel.
 *
 * Releases resources owned by this channel and its filters.
 * \param chan Pointer to the dbChannel object.
 */
DBCORE_API void dbChannelDelete(dbChannel *chan);


/** \name Other routines */

DBCORE_API void dbRegisterFilter(const char *key,
    const chFilterIf *fif, void *puser);
DBCORE_API db_field_log* dbChannelRunPreChain(dbChannel *chan,
    db_field_log *pLogIn);
DBCORE_API db_field_log* dbChannelRunPostChain(dbChannel *chan,
    db_field_log *pLogIn);
DBCORE_API const chFilterPlugin * dbFindFilter(const char *key, size_t len);
DBCORE_API void dbChannelGetArrayInfo(dbChannel *chan,
        void **pfield, long *no_elements, long *offset);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbChannel_H */
