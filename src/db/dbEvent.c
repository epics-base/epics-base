/* dbEvent.c */
/* $Id$ */
/* routines for scheduling events to lower priority tasks via the RT kernel */
/*
 * 	Author: 	Jeffrey O. Hill 
 *      Date:            4-1-89
*/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

/* Modification Log:
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
 * joh  22	011397 	designed and installed fix for "events lost"
 *			problem
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "epicsAssert.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "osiSem.h"
#include "osiThread.h"
#include "osiClock.h"
#include "errlog.h"
#include "taskwd.h"
#include "freeList.h"
#include  "tsDefs.h"
#include "dbCommon.h"
#include "dbAccess.h"
#include  "dbEvent.h"
#include  "caeventmask.h"

/* local function declarations */
LOCAL int event_read(struct event_que *ev_que);

LOCAL int db_post_single_event_private(struct event_block *event);

/* what to do with unrecoverable errors */
#define abort(S) cantProceed("dbEvent abort")

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

#define	RNGSPACE(EVQ)\
( 	( ((EVQ)->getix)>((EVQ)->putix) ) ? \
	( ((EVQ)->getix)-((EVQ)->putix) ) : \
	( ( EVENTQUESIZE+((EVQ)->getix) )- ((EVQ)->putix) ) \
)

#define LOCKEVQUE(EV_QUE)\
semMutexMustTake((EV_QUE)->writelock);

#define UNLOCKEVQUE(EV_QUE)\
semMutexGive((EV_QUE)->writelock);

#define LOCKREC(RECPTR)\
semMutexMustTake((RECPTR)->mlok);

#define UNLOCKREC(RECPTR)\
semMutexGive((RECPTR)->mlok);

LOCAL void *dbevEventUserFreeList;
LOCAL void *dbevEventQueueFreeList;

static char *EVENT_PEND_NAME = "eventTask";


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
  	if(status!=0)
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
		printf(" ev %lx\n", (unsigned long) pevent);
		printf(" ev que %lx\n", (unsigned long) pevent->ev_que);
		printf(" ev user %lx\n", (unsigned long) pevent->ev_que->evUser);
		printf("ring space %u\n", RNGSPACE(pevent->ev_que));
#endif
		printf(	"task %p select %x pfield %lx behind by %ld\n",
			pevent->ev_que->evUser->taskid,
			pevent->select,
			(unsigned long) pevent->paddr->pfield,
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
  	struct event_user	*evUser;

	if (!dbevEventUserFreeList) {
		freeListInitPvt(&dbevEventUserFreeList, 
			sizeof(struct event_user),8);
	}
	if (!dbevEventQueueFreeList) {
		freeListInitPvt(&dbevEventQueueFreeList, 
			sizeof(struct event_que),8);
	}

  	evUser = (struct event_user *) freeListCalloc(dbevEventUserFreeList);
  	if(!evUser) return NULL;
  	evUser->firstque.evUser = evUser;
        evUser->firstque.writelock = semMutexCreate();
        if(!evUser->firstque.writelock) {
            freeListFree(dbevEventUserFreeList, evUser);
            return NULL;
        }
        evUser->ppendsem = semBinaryCreate(semEmpty);
        if(!evUser->ppendsem){
            semMutexDestroy(evUser->firstque.writelock);
            freeListFree(dbevEventUserFreeList, evUser);
            return NULL;
        }
        evUser->pflush_sem = semBinaryCreate(semEmpty);
        if(!evUser->pflush_sem){
            semBinaryDestroy(evUser->ppendsem);
            semMutexDestroy(evUser->firstque.writelock);
            freeListFree(dbevEventUserFreeList, evUser);
            return NULL;
        }
        evUser->flowCtrlMode = FALSE;
        return evUser;
}


/*
 *	DB_CLOSE_EVENTS()
 *	
 *	evUser block and additional event queues
 *	deallocated when the event thread terminates
 *	itself
 *
 */
int	db_close_events(struct event_user *evUser)
{
	/*
	 * Exit not forced on event blocks for now - this is left to channel
	 * access and any other tasks using this facility which can find them
	 * more efficiently.
	 * 
	 * NOTE: not deleting events before calling this routine could be
	 * hazardous to the system's health.
	 */

  	evUser->pendexit = TRUE;

  	/* notify the waiting task */
  	semBinaryGive(evUser->ppendsem);


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
struct event_user	*evUser,
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
  	ev_que = &evUser->firstque;
  	while (TRUE) {
    		if(ev_que->quota < EVENTQUESIZE - EVENTENTRIES)
      			break;
    		if(!ev_que->nextque){
      			tmp_que = (struct event_que *) 
				freeListCalloc(dbevEventQueueFreeList);
      			if(!tmp_que) 
        			return ERROR;
      			tmp_que->evUser = evUser;
                tmp_que->writelock = semMutexCreate();
                if (!tmp_que->writelock) {
                    freeListFree (dbevEventQueueFreeList, tmp_que);
                    return ERROR;
                }
      			tmp_que->nDuplicates = 0u;
      			ev_que->nextque = tmp_que;
      			ev_que = tmp_que;
      			break;
    		}
    		ev_que = ev_que->nextque;
  	}

  	if (!pevent) {
    		pevent = (struct event_block *) malloc(sizeof(*pevent));
    		if(!pevent)
      			return ERROR;
  	}

  	pevent->npend = 	0ul;
  	pevent->user_sub = 	user_sub;
  	pevent->user_arg = 	user_arg;
  	pevent->paddr = 	paddr;
  	pevent->select = 	select;
	pevent->pLastLog = 	NULL; /* not yet in the queue */
	pevent->callBackInProgress = FALSE;

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
  	struct dbCommon *precord;
 	int status;

  	precord = (struct dbCommon *) pevent->paddr->precord;

  	LOCKREC(precord);
  	/* dont let a misplaced event corrupt the queue */
  	status = ellFind((ELLLIST*)&precord->mlis, (ELLNODE*)pevent);
  	if(status!=ERROR)
    		ellDelete((ELLLIST*)&precord->mlis, (ELLNODE*)pevent);
  	UNLOCKREC(precord);

	/*
	 * flag the event as canceled by NULLing out the callback handler 
	 *
	 * make certain that the event isnt being accessed while
	 * its call back changes
	 */
	LOCKEVQUE(pevent->ev_que)
	pevent->user_sub = NULL;
  	while (pevent->npend || pevent->callBackInProgress) {
		UNLOCKEVQUE(pevent->ev_que)
		semBinaryTakeTimeout(pevent->ev_que->evUser->pflush_sem, 1.0);
		LOCKEVQUE(pevent->ev_que)
  	}
	UNLOCKEVQUE(pevent->ev_que)

	/*
	 * Decrement event que quota
	 */
  	pevent->ev_que->quota -= EVENTENTRIES;

  	return OK;
}


/*
 * DB_ADD_OVERFLOW_EVENT()
 *
 * Specify a routine to be executed for event que overflow condition
 */
int db_add_overflow_event(
struct event_user	*evUser,
OVRFFUNC		overflow_sub,
void			*overflow_arg
)
{

  	evUser->overflow_sub = overflow_sub;
  	evUser->overflow_arg = overflow_arg;

  	return OK;
}


/*
 * DB_FLUSH_EXTRA_LABOR_EVENT()
 *
 * waits for extra labor in progress to finish
 */
int db_flush_extra_labor_event(
struct event_user	*evUser
)
{
	while(evUser->extra_labor){
		threadSleep(1.0);
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
struct event_user	*evUser,
EXTRALABORFUNC		func,
void			*arg
)
{
  	evUser->extralabor_sub = func;
  	evUser->extralabor_arg = arg;

  	return OK;
}


/*
 * 	DB_POST_EXTRA_LABOR()
 */
int db_post_extra_labor(struct event_user *evUser)
{
    	/* notify the event handler of extra labor */
	evUser->extra_labor = TRUE;
    	semBinaryGive(evUser->ppendsem);
	return OK;
}


/*
 *	DB_POST_EVENTS()
 *
 *	NOTE: This assumes that the db scan lock is already applied
 *
 */
int db_post_events(
void		*prec,
void		*pval,
unsigned int	select
)
{  
	struct dbCommon		*precord = (struct dbCommon *)prec;
	struct event_block	*event;

	if (precord->mlis.count == 0) return OK;		/* no monitors set */

  	LOCKREC(precord);
  
  	for(	event = (struct event_block *) precord->mlis.node.next;
		event;
		event = (struct event_block *) event->node.next){
		
		/*
		 * Only send event msg if they are waiting on the field which
		 * changed or pval==NULL and waiting on alarms and alarms changed
		 */
		if ( (event->paddr->pfield == (void *)pval || pval==NULL) &&
			(select & event->select)) {

			db_post_single_event_private(event);
		}
  	}

  	UNLOCKREC(precord);
  	return OK;

}


/*
 *	DB_POST_SINGLE_EVENT()
 *
 */
int db_post_single_event(struct event_block *event)
{  
	struct dbCommon *precord = event->paddr->precord;
	int status;

	dbScanLock(precord);
	status = db_post_single_event_private(event);
	dbScanUnlock(precord);
	return status;
}


/*
 *	DB_POST_SINGLE_EVENT_PRIVATE()
 *
 *	NOTE: This assumes that the db scan lock is already applied
 */
LOCAL int db_post_single_event_private(struct event_block *event)
{  
  	struct event_que	*ev_que;
	db_field_log 		*pLog;
	int 			firstEventFlag;
	unsigned		rngSpace;

	ev_que = event->ev_que;

	/*
	 * evUser ring buffer must be locked for the multiple
	 * threads writing/reading it
	 */

	LOCKEVQUE(ev_que)

	/*
	 * if we have an event on the queue and we are
	 * not saving the current value (because this is a
	 * string or an array) then ignore duplicate 
	 * events (saving them without the current valuye
	 * serves no purpose)
	 */
	if (!event->valque && event->npend>0u) {
		UNLOCKEVQUE(ev_que)
		return OK;
	}

	/* 
	 * add to task local event que 
	 */

	/*
	 * if an event is on the queue and one of
	 * {flowCtrlMode, not room for one more of each monitor attached}
	 * then replace the last event on the queue (for this monitor)
	 */
	rngSpace = RNGSPACE(ev_que);
	if ( event->npend>0u && 
		(ev_que->evUser->flowCtrlMode || rngSpace<=EVENTSPERQUE) ) {
		/*
		 * replace last event if no space is left
		 */
		pLog = event->pLastLog;
		/*
		 * the event task has already been notified about 
		 * this so we dont need to post the semaphore
		 */
		firstEventFlag = 0;
	}
	/*
	 * otherwise if the current entry is available then
	 * fill it in and advance the ring buffer 
	 */
	else if ( ev_que->evque[ev_que->putix] == EVENTQEMPTY ) {
		
		pLog = &ev_que->valque[ev_que->putix];
		ev_que->evque[ev_que->putix] = event;
		if (event->npend>0u) {
			ev_que->nDuplicates++;
		}
		event->npend++;
		/* 
		 * if the ring buffer was empty before 
		 * adding this event 
		 */
		if (rngSpace==EVENTQUESIZE) {
			firstEventFlag = 1;
		}
		else {
			firstEventFlag = 0;
		}
		ev_que->putix = RNGINC(ev_que->putix);
	}
	else {
		/*
		 * this should never occur if this is bug free 
		 */
		ev_que->evUser->queovr++;
		pLog = NULL;
		firstEventFlag = 0;
	}

	if (pLog && event->valque) {
		struct dbCommon *precord = event->paddr->precord;
		pLog->stat = precord->stat;
		pLog->sevr = precord->sevr;
		pLog->time = precord->time;

		/*
		 * use memcpy to avoid a bus error on
		 * union copy of char in the db at an odd 
		 * address
		 */
		memcpy(	(char *)&pLog->field,
			(char *)event->paddr->pfield,
			event->paddr->field_size);

		event->pLastLog = pLog;
	}

	UNLOCKEVQUE(ev_que)

	/* 
	 * its more efficent to notify the event handler 
	 * only after the event is ready and the lock
	 * is off in case it runs at a higher priority
	 * than the caller here.
	 */
	if (firstEventFlag) {
		/* 
		 * notify the event handler 
		 */
		semBinaryGive(ev_que->evUser->ppendsem);
	}

	if (pLog) {
		return OK;
	}
	else {
		return ERROR;
	}
} 


/*
 * DB_START_EVENTS()
 *
 */
int db_start_events(
struct event_user	*evUser,
char			*taskname,	/* defaulted if NULL */
int			(*init_func)(threadId),
threadId                init_func_arg,
int			priority_offset
)
{
	int		taskpri;
        int		firstTry;

  	/* only one ca_pend_event thread may be started for each evUser ! */
        threadLockContextSwitch();
        if(evUser->pendlck==0) {
            firstTry = TRUE;
            ++evUser->pendlck ;
        } else {
            firstTry = FALSE;
        }
        threadUnlockContextSwitch();
        if(!firstTry) return ERROR;
        taskpri = threadGetPriority(threadGetIdSelf());
  	taskpri -= priority_offset;
  	evUser->pendexit = FALSE;
        evUser->init_func = init_func;
        evUser->init_func_arg = init_func_arg;
  	if(!taskname) taskname = EVENT_PEND_NAME;
        evUser->taskid = threadCreate(
            taskname,taskpri,threadGetStackSize(threadStackMedium),
            (THREADFUNC)event_task,(void *)evUser);
  	return OK;
}



/*
 * EVENT_TASK()
 *
 */
void event_task( struct event_user *evUser)
{
	int			status;
  	struct event_que	*ev_que;

  	/* init hook */
  	if (evUser->init_func) {
    		status = (*evUser->init_func)(evUser->init_func_arg);
		if (status!=OK) {
			errlogPrintf("Unable to intialize the event system!\n");
			semBinaryGive(evUser->ppendsem);
  			evUser->pendexit = TRUE;
		}
	}

	taskwdInsert(threadGetIdSelf(),NULL,NULL);

  	do{
		semBinaryMustTake(evUser->ppendsem);

		/*
		 * check to see if the caller has offloaded
		 * labor to this task
		 */
		if(evUser->extra_labor && evUser->extralabor_sub){
			evUser->extra_labor = FALSE;
			(*evUser->extralabor_sub)(evUser->extralabor_arg);
		}

		for(	ev_que= &evUser->firstque; 
			ev_que; 
			ev_que = ev_que->nextque){
			event_read(ev_que);
		}

		/*
		 * The following do not introduce event latency since they
		 * are not between notification and posting events.
		 */
		if(evUser->queovr){
			if(evUser->overflow_sub)
				(*evUser->overflow_sub)(
					evUser->overflow_arg, 
					evUser->queovr);
			else
				errlogPrintf("Events lost, discard count was %d\n",
					evUser->queovr);
			evUser->queovr = 0;
		}
  	}while(!evUser->pendexit);

  	evUser->pendlck = FALSE;

        semMutexDestroy(evUser->firstque.writelock);

  	/* joh- added this code to free additional event queues */
  	{
    		struct event_que	*nextque;

		ev_que = evUser->firstque.nextque;
    		while(ev_que){ 

      			nextque = ev_que->nextque;
			semMutexDestroy(ev_que->writelock);
			freeListFree(dbevEventQueueFreeList, ev_que);
 			ev_que = nextque;
   		}
  	}

	semBinaryDestroy(evUser->ppendsem);
	semBinaryDestroy(evUser->pflush_sem);

	freeListFree(dbevEventUserFreeList, evUser);

	taskwdRemove(threadGetIdSelf());

  	return;
}


/*
 * EVENT_READ()
 *
 */
LOCAL int event_read (struct event_que *ev_que)
{
  	struct event_block *event;
  	unsigned int nextgetix;
	db_field_log *pfl;
	void (*user_sub) (void *user_arg, struct dbAddr *paddr, 
			int eventsRemaining, db_field_log *pfl);


    
	/*
	 * evUser ring buffer must be locked for the multiple
	 * threads writing/reading it
	 */
      	LOCKEVQUE(ev_que)
      	
	/*
	 * if in flow control mode drain duplicates and then
	 * suspend processing events until flow control
	 * mode is over
	 */
        if (ev_que->evUser->flowCtrlMode && ev_que->nDuplicates==0u) {
	    UNLOCKEVQUE(ev_que);
            return OK;
        }
    
	/*
	 * Fetch fast register copy
	 */
  	for(	event=ev_que->evque[ev_que->getix];
     		(event) != EVENTQEMPTY;
     		ev_que->getix = nextgetix, event = ev_que->evque[nextgetix]){

		db_field_log fl = ev_que->valque[ev_que->getix];

		/*
		 * So I can tell em if more are comming
		 */
		nextgetix = RNGINC(ev_que->getix);


		/*
		 * Simple type values queued up for reliable interprocess
		 * communication. (for other types they get whatever happens
		 * to be there upon wakeup)
		 */
		if (event->valque) {
			pfl = &fl;
		}
		else {
			pfl = NULL;
		}

    		ev_que->evque[ev_que->getix] = EVENTQEMPTY;

		/*
		 * remove event from the queue
		 */
		if (event->npend==1u) {
			event->pLastLog = NULL;
		}
		else {
			assert (event->npend>1u);
			assert (ev_que->nDuplicates>=1u);
			ev_que->nDuplicates--;
		}
 
		/*
		 * this provides a way to test to see if an event is in use
		 * despite the fact that the event queue does not point to this event
		 */
		event->callBackInProgress = TRUE;

		/*
		 * it is essential that the npend count is not lowered
		 * before the callBackInProgress flag is set
		 */
   		event->npend--;

		/*
		 * create a local copy of the call back parameters while
		 * we still have the lock
		 */
		user_sub = event->user_sub;

		/*
		 * Next event pointer can be used by event tasks to determine
		 * if more events are waiting in the queue
		 *
		 * Must remove the lock here so that we dont deadlock if
		 * this calls dbGetField() and blocks on the record lock, 
		 * dbPutField() is in progress in another task, it has the 
		 * record lock, and it is calling db_post_events() waiting 
		 * for the event queue lock (which this thread now has).
		 */
		if (user_sub != NULL) {
			UNLOCKEVQUE(ev_que)
			(*user_sub) (event->user_arg, event->paddr, 
				ev_que->evque[nextgetix]?TRUE:FALSE, pfl);
			LOCKEVQUE(ev_que)
		}

		/*
		 * this provides a way to test to see if an event is in use
		 * despite the fact that the event queue does not point to this event
		 */
		event->callBackInProgress = FALSE;

		/*
		 * check to see if this event has been canceled each
		 * time that the callBackInProgress flag is set to false
		 * while we have the event queue lock, and post the flush
		 * complete sem if there are no longer any events on the
		 * queue
		 */
		if (event->user_sub==NULL && event->npend==0u) {
			semBinaryGive (ev_que->evUser->pflush_sem);
		}
  	}

	UNLOCKEVQUE(ev_que)

  	return OK;
}

/*
 * db_event_flow_ctrl_mode_on()
 */
void db_event_flow_ctrl_mode_on (struct event_user *evUser)
{
	evUser->flowCtrlMode = TRUE;
	/* 
	 * notify the event handler task
	 */
	semBinaryGive(evUser->ppendsem);
#ifdef DEBUG
	printf("fc on %lu\n", tickGet());
#endif
}

/*
 * db_event_flow_ctrl_mode_off()
 */
void db_event_flow_ctrl_mode_off (struct event_user *evUser)
{
	evUser->flowCtrlMode = FALSE;
	/* 
	 * notify the event handler task
	 */
	semBinaryGive (evUser->ppendsem);
#ifdef DEBUG
	printf("fc off %lu\n", tickGet());
#endif
}


