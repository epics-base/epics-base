/* $Id$
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
 * Modification Log:
 * -----------------
 * .01 021494 joh	updated ANSI func proto and added extra labor
 *			call back
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

struct event_block{
        ELLNODE                 node;
        struct db_addr          *paddr;
	void 			(*user_sub)(
					void *user_arg,
					struct db_addr *paddr,
					int eventsRemaining,
					db_field_log *pfl);
        void                    *user_arg;
        struct event_que        *ev_que;
        unsigned char           select;
        char                    valque;
        unsigned long           npend;  /* n times this event is on the que */
};

typedef void			EVENTFUNC(
					void *user_arg, 
					struct db_addr *paddr,
					int eventsRemaining,
					db_field_log *pfl);


#define EVENTQUESIZE    EVENTENTRIES  *32
#define EVENTENTRIES    16      /* the number of que entries for each event */
#define EVENTQEMPTY     ((struct event_block *)NULL)


/*
 * really a ring buffer
 */
struct event_que{
        struct event_block      *evque[EVENTQUESIZE];
        db_field_log            valque[EVENTQUESIZE];
        unsigned short  putix;
        unsigned short  getix;
        unsigned short  quota;          /* the number of assigned entries*/

        /* lock writers to the ring buffer only */
        /* readers must never slow up writers */
        FAST_LOCK               writelock;

        struct event_que        *nextque;       /* in case que quota exceeded */
        struct event_user       *evuser;        /* event user parent struct */
};

struct event_user{
        int                     taskid;         /* event handler task id */

        char                    pendlck;        /* Only one task can pend */
        SEM_ID                  ppendsem;       /* Wait while empty */
        SEM_ID             	pflush_sem;	/* wait for flush */
        unsigned char           pendexit;       /* exit pend task */
	unsigned char		extra_labor;	/* if set call extra labor func */

        unsigned short  queovr;                 /* event que overflow count */
	void			(*overflow_sub)(/* called when overflow detect */
					void *overflow_arg, unsigned count);
        void                    *overflow_arg;  /* parameter to above   */

	void			(*extralabor_sub)/* off load to event task */
					(void *extralabor_arg);	
	void			*extralabor_arg;/* parameter to above */

        struct event_que        firstque;       /* the first event que */
};

typedef void			OVRFFUNC(void *overflow_arg, unsigned count);
typedef void			EXTRALABORFUNC(void *extralabor_arg);

int 			db_event_list(char *name);
struct event_user 	*db_init_events(void);
int			db_close_events(struct event_user *evuser);
unsigned		db_sizeof_event_block(void);

int db_add_event(
struct event_user       *evuser,
struct db_addr          *paddr,
EVENTFUNC		*user_sub,
void                    *user_arg,
unsigned int            select,
struct event_block      *pevent  /* ptr to event blk (not required) */
);

int     		db_cancel_event(struct event_block      *pevent);

int db_add_overflow_event(
struct event_user       *evuser,
OVRFFUNC		*overflow_sub,
void                    *overflow_arg 
);

int db_add_extra_labor_event(
struct event_user       *evuser,
EXTRALABORFUNC		*func,
void			*arg);

int db_flush_extra_labor_event(
struct event_user       *evuser
);

int 			db_post_single_event(struct event_block *pevent);

int db_post_extra_labor(struct event_user *evuser);

int db_post_events(
void			*precord,
void			*pvalue,
unsigned int            select 
);

int db_start_events(
struct event_user       *evuser,
char                    *taskname,      /* defaulted if NULL */
void                    (*init_func)(int),
int                     init_func_arg,
int                     priority_offset
);

int event_task(
struct event_user       *evuser,
void                    (*init_func)(int),
int                     init_func_arg
);

int db_event_enable(struct event_block      *pevent);
int db_event_disable(struct event_block      *pevent);

#endif /*INCLdbEventh*/

