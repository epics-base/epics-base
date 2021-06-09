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

DBCORE_API int db_event_list (
    const char *name, unsigned level);
DBCORE_API int dbel (
    const char *name, unsigned level);
DBCORE_API int db_post_events (
    void *pRecord, void *pField, unsigned caEventMask );

typedef void * dbEventCtx;

typedef void EXTRALABORFUNC (void *extralabor_arg);
DBCORE_API dbEventCtx db_init_events (void);
DBCORE_API int db_start_events (
    dbEventCtx ctx, const char *taskname, void (*init_func)(void *),
    void *init_func_arg, unsigned osiPriority );
DBCORE_API void db_close_events (dbEventCtx ctx);
DBCORE_API void db_event_flow_ctrl_mode_on (dbEventCtx ctx);
DBCORE_API void db_event_flow_ctrl_mode_off (dbEventCtx ctx);
DBCORE_API int db_add_extra_labor_event (
    dbEventCtx ctx, EXTRALABORFUNC *func, void *arg);
DBCORE_API void db_flush_extra_labor_event (dbEventCtx);
DBCORE_API int db_post_extra_labor (dbEventCtx ctx);
DBCORE_API void db_event_change_priority ( dbEventCtx ctx, unsigned epicsPriority );

#ifdef EPICS_PRIVATE_API
DBCORE_API void db_cleanup_events(void);
DBCORE_API void db_init_event_freelists (void);
#endif

typedef void EVENTFUNC (void *user_arg, struct dbChannel *chan,
    int eventsRemaining, struct db_field_log *pfl);

typedef void * dbEventSubscription;
DBCORE_API dbEventSubscription db_add_event (
    dbEventCtx ctx, struct dbChannel *chan,
    EVENTFUNC *user_sub, void *user_arg, unsigned select);
DBCORE_API void db_cancel_event (dbEventSubscription es);
DBCORE_API void db_post_single_event (dbEventSubscription es);
DBCORE_API void db_event_enable (dbEventSubscription es);
DBCORE_API void db_event_disable (dbEventSubscription es);

DBCORE_API struct db_field_log* db_create_event_log (struct evSubscrip *pevent);
DBCORE_API struct db_field_log* db_create_read_log (struct dbChannel *chan);
DBCORE_API void db_delete_field_log (struct db_field_log *pfl);
DBCORE_API int db_available_logs(void);

#define DB_EVENT_OK 0
#define DB_EVENT_ERROR (-1)

#ifdef __cplusplus
}
#endif

#endif /*INCLdbEventh*/
