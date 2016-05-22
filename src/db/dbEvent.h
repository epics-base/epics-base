/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 *      Author: Jeff Hill 
 *      Date: 	030393 
 */

#ifndef INCLdbEventh
#define INCLdbEventh

#ifdef epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#   define INCLdbEventhExporting
#endif

#include "epicsThread.h"

#ifdef INCLdbEventhExporting
#   define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbAddr;
struct db_field_log;

epicsShareFunc int epicsShareAPI db_event_list (
    const char *name, unsigned level);
epicsShareFunc int epicsShareAPI dbel (
    const char *name, unsigned level);
epicsShareFunc int epicsShareAPI db_post_events (
    void *pRecord, void *pField, unsigned caEventMask );

typedef void * dbEventCtx;

typedef void EXTRALABORFUNC (void *extralabor_arg);
epicsShareFunc dbEventCtx epicsShareAPI db_init_events (void);
epicsShareFunc int epicsShareAPI db_start_events (
    dbEventCtx ctx, const char *taskname, void (*init_func)(void *), 
    void *init_func_arg, unsigned osiPriority );
epicsShareFunc void epicsShareAPI db_close_events (dbEventCtx ctx);
epicsShareFunc void epicsShareAPI db_event_flow_ctrl_mode_on (dbEventCtx ctx);
epicsShareFunc void epicsShareAPI db_event_flow_ctrl_mode_off (dbEventCtx ctx);
epicsShareFunc int epicsShareAPI db_add_extra_labor_event (
    dbEventCtx ctx, EXTRALABORFUNC *func, void *arg);
epicsShareFunc void epicsShareAPI db_flush_extra_labor_event (dbEventCtx);
epicsShareFunc int epicsShareAPI db_post_extra_labor (dbEventCtx ctx);
epicsShareFunc void epicsShareAPI db_event_change_priority ( dbEventCtx ctx, unsigned epicsPriority );

typedef void EVENTFUNC (void *user_arg, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl);

typedef void * dbEventSubscription;
epicsShareFunc dbEventSubscription epicsShareAPI db_add_event (
    dbEventCtx ctx, struct dbAddr *paddr,
    EVENTFUNC *user_sub, void *user_arg, unsigned select);
epicsShareFunc void epicsShareAPI db_cancel_event (dbEventSubscription es);
epicsShareFunc void epicsShareAPI db_post_single_event (dbEventSubscription es);
epicsShareFunc void epicsShareAPI db_event_enable (dbEventSubscription es);
epicsShareFunc void epicsShareAPI db_event_disable (dbEventSubscription es);

#define DB_EVENT_OK 0
#define DB_EVENT_ERROR (-1)

#ifdef __cplusplus
}
#endif

#endif /*INCLdbEventh*/

