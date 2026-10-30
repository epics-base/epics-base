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

#ifndef INCLdbEventh
#define INCLdbEventh

#include "epicsThread.h"

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbChannel;
struct db_field_log;
struct evSubscrip;

#ifdef USE_TYPED_DBEVENT
struct dbEventContext; // use dbEventCtx
typedef struct evSubscrip* dbEventSubscription;
typedef struct dbEventContext* dbEventCtx;
#else
typedef void * dbEventSubscription;
typedef void * dbEventCtx;
#endif

DBCORE_API int db_event_list (
    const char *name, unsigned level);
DBCORE_API int dbel (
    const char *name, unsigned level);
DBCORE_API int db_post_events (
    void *pRecord, void *pField, unsigned caEventMask );

typedef void EXTRALABORFUNC (void *extralabor_arg);
/** @brief Allocate event reception context
 * @return NULL on error
 *
 * On success, call db_start_events(), and then eventually db_close_events().
 *
 * @pre Call after initHookAfterInitDatabase and before initHookBeforeFree.
 */
DBCORE_API dbEventCtx db_init_events (void);
/** @brief Start listener thread
 * @param ctx Context
 * @param taskname Thread name
 * @param init_func If not NULL, call from the newly created listener thread
 * @param init_func_arg Argument to init_func
 * @param osiPriority Thread priority.  See epicsThreadOpts::priority
 * @return DB_EVENT_OK Success.  DB_EVENT_ERROR, failed to create new thread.
 *
 * Basic lifecycle:
 *
 * @code{.cpp}
 * dbEventCtx ctxt = db_init_events();
 * assert(ctxt);
 * int ret = db_start_events(ctxt, "mymodule", NULL, NULL, 0);
 * assert(ret==DB_EVENT_OK);
 * ... create and close subscriptions
 * db_close_events(ctxt);
 * @endcode
 *
 * Flow control initially disabled, so events can be delivered.
 */
DBCORE_API int db_start_events (
    dbEventCtx ctx, const char *taskname, void (*init_func)(void *),
    void *init_func_arg, unsigned osiPriority );
/** @brief Stop and deallocate event reception context
 * @param ctx Context
 *
 * @pre Call after initHookAfterInitDatabase and before initHookBeforeFree.
 * @pre All dbEventSubscription must first be deallocated by db_cancel_event().
 * @post Joins event listener thread.
 */
DBCORE_API void db_close_events (dbEventCtx ctx);
/** @brief Enable flow control, pause event delivery
 * @param ctx Context
 */
DBCORE_API void db_event_flow_ctrl_mode_on (dbEventCtx ctx);
/** @brief Disable flow control, resume event delivery
 * @param ctx Context
 */
DBCORE_API void db_event_flow_ctrl_mode_off (dbEventCtx ctx);
/** @brief Setup/Clear extra labor callback
 * @param ctx Context
 * @param func Extra labor callback, may be NULL to clear previously set callback.
 * @param arg Argument to func
 * @return DB_EVENT_OK, always succeeds.
 *
 * Does not queue callback.  See db_post_extra_labor().
 */
DBCORE_API int db_add_extra_labor_event (
    dbEventCtx ctx, EXTRALABORFUNC *func, void *arg);
/** @brief Wait for extra labor callback.
 *
 *  @pre Do not call from event listener thread.
 *  @post extra labor queued flag is unchanged
 *
 *  To ensure completion, call should arrange that db_post_extra_labor() will
 *  not be called again.
 */
DBCORE_API void db_flush_extra_labor_event (dbEventCtx);
/** @brief Queue extra labor callback event
 * @param ctx Context
 * @return DB_EVENT_OK, always succeeds.
 *
 * Sets an internal queued flag, which still be set from a previous call.
 */
DBCORE_API int db_post_extra_labor (dbEventCtx ctx);
/** @brief Change event listener thread priority
 * @param ctx Context
 * @param epicsPriority Thread priority.  See epicsThreadOpts::priority
 */
DBCORE_API void db_event_change_priority ( dbEventCtx ctx, unsigned epicsPriority );

#ifdef EPICS_PRIVATE_API
DBCORE_API void db_cleanup_events(void);
DBCORE_API void db_init_event_freelists (void);
#endif

/** @brief Subscription event callback
 * @param user_arg private argument passed to db_add_event()
 * @param chan dbChannel passed to db_add_event()
 * @param eventsRemaining Approximate number of events remaining in queued.
 *        Not exact.  Use is discouraged.
 * @param pfl db_field_log of this event.
 *
 * An event callback is expected to call dbChannelGetField() or take equivalent action.
 * eg. explicitly lock the record and call dbChannelGet().
 *
 * Callee must _not_ delete the db_field_log.
 *
 * @since 7.0.5, @code pfl->mask @endcode may be used to detect which condition(s)
 *        triggered this event.
 */
typedef void EVENTFUNC (void *user_arg, struct dbChannel *chan,
    int eventsRemaining, struct db_field_log *pfl);

/** @brief Create subscription to channel
 * @param ctx Context
 * @param chan Channel
 * @param user_sub Event callback
 * @param user_arg Private argument
 * @param select Bit mask of DBE_VALUE and others.  See caeventmask.h
 * @return NULL on error.  On success, later call db_cancel_event()
 *
 * Creates a new subscription to the specified dbChannel.
 * Callbacks will be delivered on the listener thread of the provided Context.
 *
 * Creation does not queue any events.
 * Follow with a call to db_post_single_event() to queue an initial event.
 *
 * Subscription is initially disabled.  Call db_event_enable();
 *
 * Basic lifecycle:
 *
 * @code{.cpp}
 * static
 * void mycb(void *priv, struct dbChannel *chan, int eventsRemaining, struct db_field_log *pfl) {
 *     (void)eventsRemaining; // use not recommended
 *
 *     // dbChannelGetField() locks record.
 *     // May read value and meta-data from event (db_field_log).
 *     long ret = dbChannelGetField(chan, ..., pfl);
 * }
 * void someaction() {
 *     dbEventCtx ctx = ...; // previously created
 *     dbChannel *chan = ...;
 *     void *priv = ...;
 *
 *     dbEventSubscription sub = db_add_event(ctx, chan, mycb, priv, DBE_VALUE|DBE_ALARM);
 *     assert(sub);
 *     db_cancel_event(sub);
 * }
 * @endcode
 */
DBCORE_API dbEventSubscription db_add_event (
    dbEventCtx ctx, struct dbChannel *chan,
    EVENTFUNC *user_sub, void *user_arg, unsigned select);
/** @brief Deallocate subscription
 * @param es Subscription.  Must not be NULL.
 *
 * Synchronizes with Event Context worker thread to wait for a concurrent callback
 * to complete.
 */
DBCORE_API void db_cancel_event (dbEventSubscription es);
/** @brief Immediately attempt to queue an event with the present value
 * @param es Subscription
 *
 * Locks record and runs pre-chain of any server-side filters.
 * Such a filter may drop the new event (a well designed filter should not drop the first event).
 */
DBCORE_API void db_post_single_event (dbEventSubscription es);
/** @brief Enable subscription callback delivery
 * @param es Subscription
 */
DBCORE_API void db_event_enable (dbEventSubscription es);
/** @brief Disable subscription callback delivery
 * @param es Subscription
 *
 * Does __not__ synchronize with listener thread, pending callbacks may be delivered.
 * Use extra-labor mechanism and db_flush_extra_labor_event() to synchronize.
 */
DBCORE_API void db_event_disable (dbEventSubscription es);

/** @brief Allocate subscription update event.
 * @param pevent Subscription
 * @return NULL on allocation failure.
 */
DBCORE_API struct db_field_log* db_create_event_log (struct evSubscrip *pevent);
/** @brief Allocate "read" event.
 * @param pevent Subscription
 * @return NULL on allocation failure.
 *
 * Used by PVA or CA "GET" operations when polling the current value of a Channel.
 */
DBCORE_API struct db_field_log* db_create_read_log (struct dbChannel *chan);
/** @brief db_delete_field_log
 * @param pfl event structure.  May be NULL (no-op).
 */
DBCORE_API void db_delete_field_log (struct db_field_log *pfl);
DBCORE_API int db_available_logs(void);

#define DB_EVENT_OK 0
#define DB_EVENT_ERROR (-1)

#ifdef __cplusplus
}
#endif

/** @file dbEvent.h
 *
 *  Internal publish/subscribe mechanism of process database.
 *  Direct usage is discouraged in favor of derived interfaces,
 *  principally local and remote PVA/CA.
 *
 *  @since 7.0.8.1 New usage is recommended to define the USE_TYPED_DBEVENT C macro to select
 *                 typed arguments of some calls as opposed to void pointers.
 *
 *  @section dbeventobjects Objects
 *
 *  - Event context (dbEventCtx)
 *  - Event subscription (dbEventSubscription)
 *  - Channel (dbChannel)
 *  - Event / field log (db_field_log)
 *
 *  @section dbeventlifecycle Lifecycle
 *
 *  Usage is tied to the lifetime of the process database.
 *  Either through @ref inithooks or @ref dbunittest .
 *  db_init_events() must not be called before initHookAfterInitDatabase or testIocInitOk().
 *  db_close_events() must be called after initHookBeforeFree or testIocShutdownOk().
 *
 *  @note testMonitorCreate() and friends are provided to easy handling of
 *        subscriptions in unit tests.
 *
 *  Subscriptions associated with an Event Context must be deallocated before
 *  that Context is deallocated.
 *
 *  @section dbeventthread Concurrency
 *
 *  Each Event Context has an associated worker thread on which subscription callbacks are invoked.
 *  Some API functions implicitly synchronize with that worker thread as noted.
 *  The "extra labor" functions may be used to explicitly synchronize with this thread.
 */

#endif /*INCLdbEventh*/
