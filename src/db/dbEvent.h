/* 
 *      $Id$
 *
 *      Author: Jeff Hill 
 *      Date: 	030393 
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
 */

#ifndef INCLdbEventh
#define INCLdbEventh

#include "shareLib.h"


epicsShareFunc int epicsShareAPI db_event_list (
    const char *name, unsigned level);
epicsShareFunc int epicsShareAPI dbel (
    const char *name, unsigned level);
epicsShareFunc int epicsShareAPI db_post_events (
    void *precord, void *pvalue, unsigned select);

typedef void * dbEventCtx;

typedef void OVRFFUNC (void *overflow_arg, unsigned count);
typedef void EXTRALABORFUNC (void *extralabor_arg);
epicsShareFunc dbEventCtx epicsShareAPI db_init_events (void);
epicsShareFunc int epicsShareAPI db_start_events (
    dbEventCtx ctx, char *taskname, int (*init_func)(void *), 
    void *init_func_arg, int priority_offset);
epicsShareFunc void epicsShareAPI db_close_events (dbEventCtx ctx);
epicsShareFunc void epicsShareAPI db_event_flow_ctrl_mode_on (dbEventCtx ctx);
epicsShareFunc void epicsShareAPI db_event_flow_ctrl_mode_off (dbEventCtx ctx);
epicsShareFunc int epicsShareAPI db_add_overflow_event (
    dbEventCtx ctx, OVRFFUNC *overflow_sub, void *overflow_arg);
epicsShareFunc int epicsShareAPI db_add_extra_labor_event (
    dbEventCtx ctx, EXTRALABORFUNC *func, void *arg);
epicsShareFunc int epicsShareAPI db_flush_extra_labor_event (dbEventCtx);
epicsShareFunc int epicsShareAPI db_post_extra_labor (dbEventCtx ctx);

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

#endif /*INCLdbEventh*/

