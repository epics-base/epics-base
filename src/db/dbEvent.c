/* dbEvent.c */
/* share/src/db  $Id$ */
/* routines for scheduling events to lower priority tasks via the RT kernel */
/*
 * 	Author: 	Jeffrey O. Hill 
 *      Date:            4-1-89
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
 *	NOTES:
 *
 * Modification Log:
 * -----------------
 * joh	00	04xx89	Created
 * joh	01	043089	Init Release
 * joh	02	061489	changed DBCHK to PADDRCHK since we are 
 *			checking precord instead of pfield now.	
 * joh	03	072889	Added dynamic que size increase proportional 
 *			to nevents
 * joh	04	082189	Added init func to args of db_start_events()
 * joh	05	122189	fixed bug where event que not completely
 *			dealloated when there are many events.
 * joh	06	021690	changed names here and in dbCommon to avoid
 *			confusion for those maintaining this code
 *			(this change does not modify obj code).
 *			mlok became mlok
 *			mlst became mlis
 * joh	07	030590	improved db_event_list() diagnostic
 * joh	08	031590	improved flush wait in db_cancel_event()
 * joh	09	112790	added time stamp, alarm, and status logging
 * joh	10	112790	source cleanup
 * ???	11	????91	anl turned off paddr sanity checking
 * joh	12	082091	db_event_get_field() comented out
 * joh	13	091191	updated for v5 vxWorks
 * jba  14      112691  Added 'return NULL;' to end of db_event_list
 * jba  15      022892  ANSI C changes
 * joh  16    	111992  removed unused taskpri var
 * joh  17    	111992  added task priority offset arg to
 *             	   	db_start_events()
 * joh  18	072993	changed npend to unsigned long to be safe
 * joh  19	080393 	fixed sem timeout while waiting for marker bug	
 * joh  20	080393 	ANSI C changes
 * joh  21	080393 	added task watch dog	
 */

#include	<epicsAssert.h>

#include	<vxWorks.h>
#include	<types.h>
#include	<wdLib.h>
#include	<semLib.h>
#include	<stdioLib.h>
#include	<vxLib.h>
#include	<ellLib.h>
#include	<sysLib.h>
#include	<string.h>
#include	<logLib.h>
#include	<taskLib.h>

#include	<taskwd.h>
#include 	<tsDefs.h>
#include	<dbDefs.h>
#include	<dbCommon.h>
#include	<task_params.h>
#include	<db_access.h>
#include 	<dbEvent.h>
#include 	<caeventmask.h>

#include 	<memDebugLib.h>


/* local function declarations */
LOCAL EVENTFUNC	wake_cancel;

LOCAL int event_read(struct event_que *ev_que);

/* what to do with unrecoverable errors */
#define abort(S) taskSuspend((int)taskIdCurrent);

/*
 * Reliable intertask communication requires copying the current value of the
 * channel for later queing so 3 stepper motor steps of 10 each do not turn
 * into only 10 or 20 total steps part of the time.
 * 
 * NOTE: locks on this data structure are optimized so a test and set call is
 * made first.  If the lock is allready set then the task pends on the lock
 * pend sem.  Test and set call is much faster than a semaphore.  See
 * LOCKEVUSER.
 */


#define	RNGINC(OLD)\
((OLD)+1)>=EVENTQUESIZE ? 0 : ((OLD)+1)

#define LOCKEVQUE(EV_QUE)\
FASTLOCK(&(EV_QUE)->writelock);

#define UNLOCKEVQUE(EV_QUE)\
FASTUNLOCK(&(EV_QUE)->writelock);

#define LOCKREC(RECPTR)\
FASTLOCK(&(RECPTR)->mlok);

#define UNLOCKREC(RECPTR)\
FASTUNLOCK(&(RECPTR)->mlok);


/*
 *	DB_EVENT_LIST()
 *
 *
 */
int db_event_list(char *name)
{
  	struct dbAddr		addr;
  	int			status;
  	struct event_block	*pevent;
  	struct dbCommon		*precord;

  	status = dbNameToAddr(name, &addr);
  	if(status==ERROR)
    		return ERROR;

  	precord = addr.precord;
  	pevent = (struct event_block *) precord->mlis.node.next;

  	if(pevent)
    		printf("List of events (monitors).\n");

  	LOCKREC(precord);
    	for(	;
		pevent;
		pevent = (struct event_block *) pevent->node.next){
#ifdef DEBUG
		printf(" ev %x\n",pevent);
		printf(" ev que %x\n",pevent->ev_que);
		printf(" ev user %x\n",pevent->ev_que->evuser);
#endif
		printf(	"task %x select %x pfield %x behind by %ld\n",
			pevent->ev_que->evuser->taskid,
			pevent->select,
			(unsigned int) pevent->paddr->pfield,
			pevent->npend);
     	}
  	UNLOCKREC(precord);

	return OK;
}


/*
 * DB_INIT_EVENTS()
 *
 *
 * Initialize the event facility for this task. Must be called at least once
 * by each task which uses the db event facility
 * 
 * returns:	ptr to event user block or NULL if memory can't be allocated
 */
struct event_user *db_init_events(void)
{
  	struct event_user	*evuser;

  	evuser = (struct event_user *) calloc(1, sizeof(*evuser));
  	if(!evuser)
    		return NULL;

  	evuser->firstque.evuser = evuser;
	FASTLOCKINIT(&(evuser->firstque.writelock));
	evuser->ppendsem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!evuser->ppendsem){
		FASTLOCKFREE(&(evuser->firstque.writelock));
		free(evuser);
		return NULL;
	}
	evuser->pflush_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!evuser->pflush_sem){
		FASTLOCKFREE(&(evuser->firstque.writelock));
		semDelete(evuser->ppendsem);
		free(evuser);
		return NULL;
	}

  	return evuser;
}


/*
 *	DB_CLOSE_EVENTS()
 *	
 *	evuser block and additional event queues
 *	deallocated when the event thread terminates
 *	itself
 *
 */
int	db_close_events(struct event_user *evuser)
{
	/*
	 * Exit not forced on event blocks for now - this is left to channel
	 * access and any other tasks using this facility which can find them
	 * more efficiently.
	 * 
	 * NOTE: not deleting events before calling this routine could be
	 * hazardous to the system's health.
	 */

  	evuser->pendexit = TRUE;

  	/* notify the waiting task */
  	semGive(evuser->ppendsem);


  	return OK;
}


/*
 * DB_SIZEOF_EVENT_BLOCK()
 *
 *
 * So the event block structure size but not structure is exported (used by
 * those who wish for the event block to be a sub structure) see pevent !=
 * NULL on db_add_event()
 */
unsigned db_sizeof_event_block(void)
{
  	return sizeof(struct event_block);
}



/*
 * DB_ADD_EVENT()
 *
 *
 */
int db_add_event(
struct event_user	*evuser,
struct dbAddr		*paddr,
void			(*user_sub)(),
void			*user_arg,
unsigned int		select,
struct event_block	*pevent /* ptr to event blk (not required) */
)
{
  	struct dbCommon		*precord;
  	struct event_que	*ev_que;
  	struct event_que	*tmp_que;

/* (MDA) in LANL stuff, this used to taskSuspend if invalid address
    in new code, the mechanism to help do this checking has been removed
    (and replaced)?

  	PADDRCHK(paddr);
 */
  	precord = paddr->precord;

	/*
	 * Don't add events which will not be triggered
	 */
  	if(!select)
    		return ERROR;

  	/* find an event que block with enough quota */
  	/* otherwise add a new one to the list */
  	ev_que = &evuser->firstque;
  	while(TRUE){
    		if(ev_que->quota < EVENTQUESIZE - EVENTENTRIES)
      			break;
    		if(!ev_que->nextque){
      			tmp_que = (struct event_que *) 
				calloc(1, sizeof(*tmp_que));   

      			if(!tmp_que)
        			return ERROR;
      			tmp_que->evuser = evuser;
			FASTLOCKINIT(&(tmp_que->writelock));
      			ev_que->nextque = tmp_que;
      			ev_que = tmp_que;
      			break;
    		}
    		ev_que = ev_que->nextque;
  	}

  	if(!pevent){
    		pevent = (struct event_block *) malloc(sizeof(*pevent));
    		if(!pevent)
      			return ERROR;
  	}

  	pevent->npend = 	0;
  	pevent->user_sub = 	user_sub;
  	pevent->user_arg = 	user_arg;
  	pevent->paddr = 	paddr;
  	pevent->select = 	select;

  	ev_que->quota += 	EVENTENTRIES;
  	pevent->ev_que = 	ev_que;

	/*
	 * Simple types values queued up for reliable interprocess
	 * communication (for other types they get whatever happens to be
	 * there upon wakeup)
	 */
  	if(	paddr->no_elements == 1 && 
		paddr->field_size <= sizeof(union native_value))
  		pevent->valque = TRUE;
 	else
  		pevent->valque = FALSE;

  	LOCKREC(precord);
  	ellAdd(&precord->mlis, &pevent->node);
  	UNLOCKREC(precord);

  	return OK;
}


/*
 * db_event_enable()
 */
int db_event_enable(struct event_block      *pevent)
{
  	struct dbCommon	*precord;
	int		status;

  	precord = (struct dbCommon *) pevent->paddr->precord;

  	LOCKREC(precord);
  	/* 
	 * dont let a misplaced event corrupt the queue 
	 */
  	status = ellFind(&precord->mlis, &pevent->node);
	if(status == ERROR){
  		ellAdd(&precord->mlis, &pevent->node);
	}
  	UNLOCKREC(precord);

	if(status != ERROR){
		return ERROR;
	}
	else{
		return OK;
	}
}


/*
 * db_event_disable()
 */
int db_event_disable(struct event_block      *pevent)
{
  	struct dbCommon	*precord;
	int		status;

  	precord = (struct dbCommon *) pevent->paddr->precord;

  	LOCKREC(precord);
  	/* 
	 * dont let a misplaced event corrupt the queue 
	 */
  	status = ellFind(&precord->mlis, &pevent->node);
	if(status == OK){
  		ellDelete(&precord->mlis, &pevent->node);
	}
  	UNLOCKREC(precord);

	if(status != OK){
		return ERROR;
	}
	else{
		return OK;
	}
}


/*
 * DB_CANCEL_EVENT()
 *
 *
 * This routine does not prevent two threads from deleting one block at the
 * same time.
 * 
 * This routine does not deallocate the event block since it normally will be
 * part of a larger structure.
 */
int	db_cancel_event(struct event_block	*pevent)
{
  	struct dbCommon		*precord;
  	int			status;
	struct event_user	*pevu;

/* (MDA) in LANL stuff, this used to taskSuspend if invalid address
  	PADDRCHK(pevent->paddr);
 */

  	precord = (struct dbCommon *) pevent->paddr->precord;

  	LOCKREC(precord);
  	/* dont let a misplaced event corrupt the queue */
  	status = ellFind((ELLLIST*)&precord->mlis, (ELLNODE*)pevent);
  	if(status!=ERROR)
    		ellDelete((ELLLIST*)&precord->mlis, (ELLNODE*)pevent);
  	UNLOCKREC(precord);

	/*
	 * Flush the event que so we know event block not left in use. This
	 * requires placing a fake event which wakes this thread once the
	 * event queue has been flushed. This replaces a block of code which
	 * used to lower priority below the event thread to accomplish the
	 * flush without polling.
	 */
  	if(pevent->npend){
    		struct event_block	flush_event;

		pevu = pevent->ev_que->evuser;

    		flush_event = *pevent;
    		flush_event.user_sub = wake_cancel;
    		flush_event.user_arg = &pevu->pflush_sem;
    		flush_event.npend = 0;
    		if(db_post_single_event(&flush_event)==OK){
			/*
			 * insure that the event is 
			 * removed from the queue
			 */
    			while(flush_event.npend){
				semTake(
					pevu->pflush_sem,
					sysClkRateGet());
			}
		}

    		/* 
		 * in case the event could not be queued 
		 */
    		while(pevent->npend){
      			taskDelay(sysClkRateGet());
		}
  	}

	/*
	 * Decrement event que quota
	 */
  	pevent->ev_que->quota -= EVENTENTRIES;

  	return OK;
}


/*
 * WAKE_CANCEL()
 *
 * a very short routine to inform a db_clear thread that the deleted event
 * has been flushed
 */
LOCAL void wake_cancel(
void 			*user_arg,
struct dbAddr 		*paddr,
int			eventsRemaing,
db_field_log 		*pfl)
{
	SEM_ID  *psem;

	psem = (SEM_ID *)user_arg;
    	semGive(*psem);
}


/*
 * DB_ADD_OVERFLOW_EVENT()
 *
 * Specify a routine to be executed for event que overflow condition
 */
int db_add_overflow_event(
struct event_user	*evuser,
OVRFFUNC		overflow_sub,
void			*overflow_arg
)
{

  	evuser->overflow_sub = overflow_sub;
  	evuser->overflow_arg = overflow_arg;

  	return OK;
}


/*
 * DB_FLUSH_EXTRA_LABOR_EVENT()
 *
 * waits for extra labor in progress to finish
 */
int db_flush_extra_labor_event(
struct event_user	*evuser
)
{
	while(evuser->extra_labor){
		taskDelay(sysClkRateGet());
	}

  	return OK;
}


/*
 * DB_ADD_EXTRA_LABOR_EVENT()
 *
 * Specify a routine to be called
 * when labor is offloaded to the
 * event task
 */
int db_add_extra_labor_event(
struct event_user	*evuser,
EXTRALABORFUNC		func,
void			*arg
)
{
  	evuser->extralabor_sub = func;
  	evuser->extralabor_arg = arg;

  	return OK;
}


/*
 * 	DB_POST_EXTRA_LABOR()
 */
int db_post_extra_labor(struct event_user *evuser)
{
	int 	status;

    	/* notify the event handler of extra labor */
	evuser->extra_labor = TRUE;
    	status = semGive(evuser->ppendsem);
	assert(status == OK);

	return OK;
}


/*
 *	DB_POST_SINGLE_EVENT()
 *
 *
 */
int db_post_single_event(struct event_block	*pevent)
{  
  	struct event_que	*ev_que = pevent->ev_que;
  	int			success = FALSE;
	struct dbCommon		*precord;
  	unsigned int		putix;

	precord = (struct dbCommon *) pevent->paddr->precord;

	/*
	 * evuser ring buffer must be locked for the multiple threads writing
	 * to it
	 */
  	LOCKEVQUE(ev_que)
  	putix = ev_que->putix;

  	/* add to task local event que */
  	if(ev_que->evque[putix] == EVENTQEMPTY){
		short sevr;

    		pevent->npend++;
    		ev_que->evque[putix] = pevent;
		if(pevent->valque) {
			ev_que->valque[putix].stat = precord->stat;
			sevr = precord->sevr;
			ev_que->valque[putix].sevr = sevr;
			ev_que->valque[putix].time = precord->time;
			/*
		 	 * use memcpy to avoid a bus error on
		 	 * union copy of char in the db at an odd 
		 	 * address
		 	 */
			memcpy(	(char *)&ev_que->valque[putix].field,
				pevent->paddr->pfield,
				pevent->paddr->field_size);
		}
    		/* notify the event handler */
    		semGive(ev_que->evuser->ppendsem);
    		ev_que->putix = RNGINC(putix);
    		success = TRUE;
  	}
  	else
    		ev_que->evuser->queovr++;

  	UNLOCKEVQUE(ev_que)

  	if(success)
    		return OK;
  	else
    		return ERROR;
}


/*
 *	DB_POST_EVENTS()
 *
 *
 */
int db_post_events(
void		*prec,
void		*pval,
unsigned int	select
)
{  
	struct dbCommon		*precord = (struct dbCommon *)prec;
	union native_value	*pvalue = (union native_value *)pval;
	struct event_block	*event;
  	struct event_que	*ev_que;
 	unsigned int		putix;

	if (precord->mlis.count == 0) return OK;		/* no monitors set */

  	LOCKREC(precord);
  
  	for(	event = (struct event_block *) precord->mlis.node.next;
		event;
		event = (struct event_block *) event->node.next){

    		ev_que = event->ev_que;

		/*
		 * Only send event msg if they are waiting on the field which
		 * changed or pvalue==NULL and waiting on alarms and alarms changed
		 */
    		if( ((event->paddr->pfield == (void *)pvalue) && (select & event->select))
		    ||(pvalue==NULL && (select==DBE_ALARM && event->select==DBE_ALARM)) ){

			/*
			 * evuser ring buffer must be locked for the multiple
			 * threads writing to it
			 */

      			LOCKEVQUE(ev_que)
      			putix = ev_que->putix;

      			/* add to task local event que */
      			if(ev_que->evque[putix] == EVENTQEMPTY){
				short sevr;

				event->npend++;
        			ev_que->evque[putix] = event;
				if(event->valque) {
					ev_que->valque[putix].stat = precord->stat;
					sevr = precord->sevr;
					ev_que->valque[putix].sevr = sevr;
					ev_que->valque[putix].time = precord->time;

					/*
				 	* use memcpy to avoid a bus error on
				 	* union copy of char in the db at an odd 
				 	* address
				 	*/
					memcpy(	(char *)&ev_que->valque[putix].field,
						(char *)event->paddr->pfield,
						event->paddr->field_size);

				}
        			/* notify the event handler */
        			semGive(ev_que->evuser->ppendsem);

        			ev_que->putix = RNGINC(putix);
      			}
      			else
        			ev_que->evuser->queovr++;

      			UNLOCKEVQUE(ev_que)

    		}
    
  	}

  	UNLOCKREC(precord);
  	return OK;

}


/*
 * DB_START_EVENTS()
 *
 */
int db_start_events(
struct event_user	*evuser,
char			*taskname,	/* defaulted if NULL */
void			(*init_func)(),
int			init_func_arg,
int			priority_offset
)
{
  	int		status;
	int		taskpri;
  	int		event_task();

  	/* only one ca_pend_event thread may be started for each evuser ! */
  	while(!vxTas(&evuser->pendlck))
    		return ERROR;

  	status = taskPriorityGet(taskIdSelf(), &taskpri);
  	if(status == ERROR)
    		return ERROR;
 
  	taskpri += priority_offset;

  	evuser->pendexit = FALSE;

  	if(!taskname)
    		taskname = EVENT_PEND_NAME;
  	status = 
    	taskSpawn(	
		taskname,
		taskpri,
		EVENT_PEND_OPT,
		EVENT_PEND_STACK,
		event_task,
		(int)evuser,
		(int)init_func,
		(int)init_func_arg,
		0,0,0,0,0,0,0);
  	if(status == ERROR)
    		return ERROR;

  	evuser->taskid = status;

  	return OK;
}



/*
 * EVENT_TASK()
 *
 */
int event_task(
struct event_user	*evuser,
void			(*init_func)(),
int			init_func_arg
)
{
	int			status;
  	struct event_que	*ev_que;

	taskwdInsert((int)taskIdCurrent,NULL,NULL);

  	/* init hook */
  	if(init_func)
    		(*init_func)(init_func_arg);

	/*
	 * No need to lock getix as I only allow one thread to call this
	 * routine at a time
	 */
  	do{
		semTake(evuser->ppendsem, WAIT_FOREVER);

		/*
		 * check to see if the caller has offloaded
		 * labor to this task
		 */
		if(evuser->extra_labor && evuser->extralabor_sub){
			evuser->extra_labor = FALSE;
			(*evuser->extralabor_sub)(evuser->extralabor_arg);
		}

    		for(	ev_que= &evuser->firstque; 
			ev_que; 
			ev_que = ev_que->nextque){

      			event_read(ev_que);
		}

		/*
		 * The following do not introduce event latency since they
		 * are not between notification and posting events.
		 */
    		if(evuser->queovr){
      			if(evuser->overflow_sub)
        			(*evuser->overflow_sub)(
					evuser->overflow_arg, 
					evuser->queovr);
      			else
				logMsg("Events lost, discard count was %d\n",
					evuser->queovr,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
      			evuser->queovr = 0;
    		}

  	}while(!evuser->pendexit);

  	evuser->pendlck = FALSE;

	if(FASTLOCKFREE(&evuser->firstque.writelock)<0)
		logMsg("evtsk: fast lock free fail 1\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);

  	/* joh- added this code to free additional event queues */
  	{
    		struct event_que	*nextque;

		ev_que = evuser->firstque.nextque;
    		while(ev_que){ 

      			nextque = ev_que->nextque;
			if(FASTLOCKFREE(&ev_que->writelock)<0)
				logMsg("evtsk: fast lock free fail 2\n",
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
      			free(ev_que);
 			ev_que = nextque;
   		}
  	}

	status = semDelete(evuser->ppendsem);
	if(status != OK){
		logMsg("evtsk: sem delete fail at exit\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}
	status = semDelete(evuser->pflush_sem);
	if(status != OK){
		logMsg("evtsk: flush sem delete fail at exit\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

  	free(evuser);

	taskwdRemove((int)taskIdCurrent);

  	return OK;
}


/*
 * EVENT_READ()
 *
 */
LOCAL int event_read(struct event_que       *ev_que)
{
  	struct event_block	*event;
 	unsigned int		getix;
  	unsigned int		nextgetix;
	db_field_log		*pfl;


	/*
	 * Fetch fast register copy
	 */
  	getix = ev_que->getix;

	/*
	 * No need to lock getix as I only allow one thread to call this
	 * routine at a time
	 */

  	for(	event=ev_que->evque[getix];
     		(event) != EVENTQEMPTY;
     		getix = nextgetix, event = ev_que->evque[nextgetix]){

		/*
		 * So I can tell em if more are comming
		 */

    		nextgetix = RNGINC(getix);


		/*
		 * Simple type values queued up for reliable interprocess
		 * communication. (for other types they get whatever happens
		 * to be there upon wakeup)
		 */
		if(event->valque)
			pfl = &ev_que->valque[getix];
		else
			pfl = NULL;

		/*
		 * Next event pointer can be used by event tasks to determine
		 * if more events are waiting in the queue
		 */
    		(*event->user_sub)(
				event->user_arg, 
				event->paddr,
				ev_que->evque[nextgetix]?TRUE:FALSE,
				pfl);

    		ev_que->evque[getix] = (struct event_block *) NULL;
    		event->npend--;

  	}

	/*
	 * Store back to RAM copy
	 */
  	ev_que->getix = getix;

  	return OK;
}

