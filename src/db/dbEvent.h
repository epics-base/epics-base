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

/*
 * collides with db_access.h used in the CA client
 */
#ifndef caClient
#include <dbCommon.h>
#endif /*caClient*/

#include <db_field_log.h>
#include <osiThread.h>

int db_event_list (const char *name, unsigned level);
int dbel (const char *name, unsigned level);
int db_post_events (void *precord, void *pvalue, unsigned select);

typedef void * dbEventCtx;

typedef void OVRFFUNC (void *overflow_arg, unsigned count);
typedef void EXTRALABORFUNC (void *extralabor_arg);
dbEventCtx db_init_events (void);
int db_start_events (dbEventCtx ctx, char *taskname, int (*init_func)(threadId), 
                     threadId init_func_arg, int priority_offset);
void db_close_events (dbEventCtx ctx);
void db_event_flow_ctrl_mode_on (dbEventCtx ctx);
void db_event_flow_ctrl_mode_off (dbEventCtx ctx);
int db_add_overflow_event (dbEventCtx ctx, OVRFFUNC *overflow_sub, 
                           void *overflow_arg);
int db_add_extra_labor_event (dbEventCtx ctx, EXTRALABORFUNC *func, void *arg);
int db_flush_extra_labor_event (dbEventCtx);
int db_post_extra_labor (dbEventCtx ctx);

typedef void EVENTFUNC (void *user_arg, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl);

typedef void * dbEventSubscription;
dbEventSubscription db_add_event (dbEventCtx ctx, struct dbAddr *paddr,
    EVENTFUNC *user_sub, void *user_arg, unsigned select);
void db_cancel_event (dbEventSubscription es);
void db_post_single_event (dbEventSubscription es);
void db_event_enable (dbEventSubscription es);
void db_event_disable (dbEventSubscription es);

#define DB_EVENT_OK 0
#define DB_EVENT_ERROR (-1)

#endif /*INCLdbEventh*/

