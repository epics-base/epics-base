/* dbEvent.c */
/* $Id$ */
/* routines for scheduling events to lower priority tasks via the RT kernel */
/*
 *  Author:     Jeffrey O. Hill 
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
#include "errlog.h"
#include "taskwd.h"
#include "freeList.h"
#include "tsStamp.h"
#include "dbCommon.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "caeventmask.h"

#define EVENTSPERQUE    32
#define EVENTQUESIZE    (EVENTENTRIES  * EVENTSPERQUE)
#define EVENTENTRIES    4      /* the number of que entries for each event */
#define EVENTQEMPTY     ((struct evSubscrip *)NULL)

/*
 * event subscruiption
 */
struct evSubscrip {
    ELLNODE                 node;
    struct dbAddr           *paddr;
    EVENTFUNC               *user_sub;
    void                    *user_arg;
    struct event_que        *ev_que;
    db_field_log            *pLastLog;
    unsigned long           npend;  /* n times this event is on the queue */
    unsigned long           nreplace;  /* n times replacing event on the queue */
    unsigned char           select;
    char                    valque;
    char                    callBackInProgress;
};

/*
 * really a ring buffer
 */
struct event_que {
    /* lock writers to the ring buffer only */
    /* readers must never slow up writers */
    semMutexId              writelock;
    db_field_log            valque[EVENTQUESIZE];
    struct evSubscrip       *evque[EVENTQUESIZE];
    struct event_que        *nextque;       /* in case que quota exceeded */
    struct event_user       *evUser;        /* event user parent struct */
    unsigned short          putix;
    unsigned short          getix;
    unsigned short          quota;          /* the number of assigned entries*/
    unsigned short          nDuplicates;    /* N events duplicated on this q */ 
};

struct event_user {
    struct event_que    firstque;       /* the first event que */
    
    semBinaryId         ppendsem;       /* Wait while empty */
    semBinaryId         pflush_sem;     /* wait for flush */
    
    OVRFFUNC            *overflow_sub;  /* called when overflow detect */
    void                *overflow_arg;  /* parameter to above   */
    
    EXTRALABORFUNC      *extralabor_sub;/* off load to event task */
    void                *extralabor_arg;/* parameter to above */
    
    threadId            taskid;         /* event handler task id */
    unsigned            queovr;         /* event que overflow count */
    unsigned char       pendexit;       /* exit pend task */
    unsigned char       extra_labor;    /* if set call extra labor func */
    unsigned char       flowCtrlMode;   /* replace existing monitor */
    int                 (*init_func)();
    threadId            init_func_arg;
};


/* what to do with unrecoverable errors */
#define abort(S) cantProceed ("dbEvent abort")

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


#define RNGINC(OLD)\
((OLD)+1)>=EVENTQUESIZE ? 0 : ((OLD)+1)

#define RNGSPACE(EVQ)\
(   ( ((EVQ)->getix)>((EVQ)->putix) ) ? \
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
LOCAL void *dbevEventBlockFreeList;

static char *EVENT_PEND_NAME = "eventTask";

/*
 *  db_event_list ()
 */
int db_event_list (const char *pname, unsigned level)
{
    return dbel (pname, level);
}

/*
 * dbel ()
 */
int dbel (const char *pname, unsigned level)
{
    DBADDR              addr;
    long                status;
    struct evSubscrip   *pevent;
    dbFldDes 		    *pdbFldDes;

    status = dbNameToAddr (pname, &addr);
    if (status!=0) {
	    errMessage (status, " dbNameToAddr failed");
        return DB_EVENT_ERROR;
    }

    LOCKREC (addr.precord);

    pevent = (struct evSubscrip *) ellFirst (&addr.precord->mlis);

    if (!pevent) {
	    printf ("\"%s\": No monitor event subscriptions.\n", pname);
        UNLOCKREC (addr.precord);
        return DB_EVENT_OK;
    }

    printf ("Monitor Event Subscriptions\n");

    while (pevent) {
	    pdbFldDes = pevent->paddr->pfldDes;

	    printf ("%4.4s", pdbFldDes->name);

        /* they should never see this one */
        if (pevent->ev_que->evUser->queovr) {
            printf (" !! joint event discard count=%ld !!", 
                pevent->ev_que->evUser->queovr);
        }

	    if (pevent->select & DBE_VALUE) printf(" VALUE");
	    if (pevent->select & DBE_LOG) printf(" LOG");
	    if (pevent->select & DBE_ALARM) printf(" ALARM");

        if (level>0) {
            if (pevent->npend) {
                printf (" undelivered=%ld\n", pevent->npend);
            }

            if (pevent->nreplace) {
                printf (" discarded by replacement=%ld\n", pevent->nreplace);
            }
        }

        if (level>1) {
            printf (" task id=%p unused entries avail=%u", 
                pevent->ev_que->evUser->taskid, RNGSPACE (pevent->ev_que));
            if (!pevent->valque) {
                printf (" queueing disabled");
            }
            printf (" joint duplicate count =%u\n", 
                pevent->ev_que->nDuplicates);
        }

        if (level>3) {
            printf (" ev %p ev que %p ev user %p\n", 
                pevent, pevent->ev_que, pevent->ev_que->evUser);
        }

	    printf("\n");

	    pevent = (struct evSubscrip *) ellNext (&pevent->node);
    }

    UNLOCKREC (addr.precord);

    return DB_EVENT_OK;
}

/*
 * DB_INIT_EVENTS()
 *
 *
 * Initialize the event facility for this task. Must be called at least once
 * by each task which uses the db event facility
 * 
 * returns: ptr to event user block or NULL if memory can't be allocated
 */
dbEventCtx db_init_events (void)
{
    struct event_user   *evUser;
    
    if (!dbevEventUserFreeList) {
        freeListInitPvt(&dbevEventUserFreeList, 
            sizeof(struct event_user),8);
    }
    if (!dbevEventQueueFreeList) {
        freeListInitPvt(&dbevEventQueueFreeList, 
            sizeof(struct event_que),8);
    }
    if (!dbevEventBlockFreeList) {
        freeListInitPvt(&dbevEventBlockFreeList, 
            sizeof(struct evSubscrip),256);
    }
    
    evUser = (struct event_user *) 
        freeListCalloc(dbevEventUserFreeList);
    if (!evUser) {
        return NULL;
    }
    
    evUser->firstque.evUser = evUser;
    evUser->firstque.writelock = semMutexCreate();
    if (!evUser->firstque.writelock) {
        return NULL;
    }

    evUser->ppendsem = semBinaryCreate(semEmpty);
    if (!evUser->ppendsem) {
        semMutexDestroy (evUser->firstque.writelock);
        return NULL;
    }    
    evUser->pflush_sem = semBinaryCreate(semEmpty);
    if (!evUser->pflush_sem) {
        semMutexDestroy (evUser->firstque.writelock);
        semBinaryDestroy (evUser->ppendsem);
        return NULL;
    }
    evUser->flowCtrlMode = FALSE;
    return (dbEventCtx) evUser;
}

/*
 *  DB_CLOSE_EVENTS()
 *  
 *  evUser block and additional event queues
 *  deallocated when the event thread terminates
 *  itself
 *
 */
void db_close_events (dbEventCtx ctx)
{
    struct event_user *evUser = (struct event_user *) ctx;

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
}

/*
 * DB_ADD_EVENT()
 */
dbEventSubscription db_add_event (dbEventCtx ctx, struct dbAddr *paddr,
    EVENTFUNC *user_sub, void *user_arg, unsigned select)
{
    struct event_user   *evUser = (struct event_user *) ctx;
    struct dbCommon     *precord;
    struct event_que    *ev_que;
    struct event_que    *tmp_que;
    struct evSubscrip  *pevent;

    precord = paddr->precord;

    /*
     * Don't add events which will not be triggered
     */
    if (!select) {
        return NULL;
    }

    pevent = freeListCalloc (dbevEventBlockFreeList);
    if (!pevent) {
        return NULL;
    }

    /* find an event que block with enough quota */
    /* otherwise add a new one to the list */
    ev_que = &evUser->firstque;
    while (TRUE) {
        if (ev_que->quota < EVENTQUESIZE - EVENTENTRIES) {
            break;
        }
        if (!ev_que->nextque) {
            tmp_que = (struct event_que *) 
                freeListCalloc(dbevEventQueueFreeList);
            if (!tmp_que) {
                freeListFree (dbevEventBlockFreeList, pevent);
                return NULL;
            }
            tmp_que->evUser = evUser;
            tmp_que->writelock = semMutexCreate();
            if (!tmp_que->writelock) {
                freeListFree (dbevEventBlockFreeList, pevent);
                freeListFree (dbevEventQueueFreeList, tmp_que);
                return NULL;
            }
            tmp_que->nDuplicates = 0u;
            ev_que->nextque = tmp_que;
            ev_que = tmp_que;
            break;
        }
        ev_que = ev_que->nextque;
    }

    pevent->npend =     0ul;
    pevent->nreplace =  0ul;
    pevent->user_sub =  user_sub;
    pevent->user_arg =  user_arg;
    pevent->paddr =     paddr;
    pevent->select =    select;
    pevent->pLastLog =  NULL; /* not yet in the queue */
    pevent->callBackInProgress = FALSE;

    ev_que->quota +=    EVENTENTRIES;
    pevent->ev_que =    ev_que;

    /*
     * Simple types values queued up for reliable interprocess
     * communication (for other types they get whatever happens to be
     * there upon wakeup)
     */
    if( paddr->no_elements == 1 && 
        paddr->field_size <= sizeof(union native_value)) {
        pevent->valque = TRUE;
    }
    else {
        pevent->valque = FALSE;
    }

    LOCKREC(precord);
    ellAdd(&precord->mlis, &pevent->node);
    UNLOCKREC(precord);

    return pevent;
}

/*
 * db_event_enable()
 */
void db_event_enable (dbEventSubscription es)
{
    struct evSubscrip *pevent = (struct evSubscrip *) es;
    struct dbCommon *precord;
    int     status;

    precord = (struct dbCommon *) pevent->paddr->precord;

    LOCKREC(precord);
    /* 
     * dont let a misplaced event corrupt the queue 
     */
    status = ellFind (&precord->mlis, &pevent->node);
    if (status) {
        ellAdd (&precord->mlis, &pevent->node);
    }
    UNLOCKREC(precord);
}

/*
 * db_event_disable()
 */
void db_event_disable (dbEventSubscription es)
{
    struct evSubscrip *pevent = (struct evSubscrip *) es;
    struct dbCommon *precord;
    int     status;

    precord = (struct dbCommon *) pevent->paddr->precord;

    LOCKREC(precord);
    /* 
     * dont let a misplaced event corrupt the queue 
     */
    status = ellFind(&precord->mlis, &pevent->node);
    if (!status) {
        ellDelete(&precord->mlis, &pevent->node);
    }
    UNLOCKREC(precord);
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
void db_cancel_event (dbEventSubscription es)
{
    struct evSubscrip *pevent = (struct evSubscrip *) es;
    struct dbCommon *precord;
    int status;

    precord = (struct dbCommon *) pevent->paddr->precord;

    LOCKREC(precord);
    /* dont let a misplaced event corrupt the queue */
    status = ellFind((ELLLIST*)&precord->mlis, &pevent->node);
    if (status==DB_EVENT_ERROR) {
        errlogPrintf ("db_cancel_event() - invalid event ignored\n");
        UNLOCKREC(precord);
        return;
    }
    ellDelete((ELLLIST*)&precord->mlis, &pevent->node);
    UNLOCKREC(precord);

    /*
     * flag the event as canceled by NULLing out the callback handler 
     *
     * make certain that the event isnt being accessed while
     * its call back changes
     */
    LOCKEVQUE (pevent->ev_que)
    pevent->user_sub = NULL;
    while (pevent->npend || pevent->callBackInProgress) {
        UNLOCKEVQUE(pevent->ev_que)
        semBinaryTakeTimeout(pevent->ev_que->evUser->pflush_sem, 1.0);
        LOCKEVQUE(pevent->ev_que)
    }
    UNLOCKEVQUE (pevent->ev_que)

    /*
     * Decrement event que quota
     */
    pevent->ev_que->quota -= EVENTENTRIES;

    freeListFree (dbevEventBlockFreeList, pevent);

    return;
}

/*
 * DB_ADD_OVERFLOW_EVENT()
 *
 * Specify a routine to be executed for event que overflow condition
 */
int db_add_overflow_event (dbEventCtx ctx, OVRFFUNC *overflow_sub, 
                           void *overflow_arg)
{
    struct event_user *evUser = (struct event_user *) ctx;

    evUser->overflow_sub = overflow_sub;
    evUser->overflow_arg = overflow_arg;

    return DB_EVENT_OK;
}

/*
 * DB_FLUSH_EXTRA_LABOR_EVENT()
 *
 * waits for extra labor in progress to finish
 */
int db_flush_extra_labor_event (dbEventCtx ctx)
{
    struct event_user *evUser = (struct event_user *) ctx;

    while(evUser->extra_labor){
        threadSleep(1.0);
    }

    return DB_EVENT_OK;
}

/*
 * DB_ADD_EXTRA_LABOR_EVENT()
 *
 * Specify a routine to be called
 * when labor is offloaded to the
 * event task
 */
int db_add_extra_labor_event (dbEventCtx ctx, 
                      EXTRALABORFUNC *func, void *arg)
{
    struct event_user *evUser = (struct event_user *) ctx;

    evUser->extralabor_sub = func;
    evUser->extralabor_arg = arg;

    return DB_EVENT_OK;
}

/*
 *  DB_POST_EXTRA_LABOR()
 */
int db_post_extra_labor (dbEventCtx ctx)
{
    struct event_user *evUser = (struct event_user *) ctx;

    /* notify the event handler of extra labor */
    evUser->extra_labor = TRUE;
    semBinaryGive(evUser->ppendsem);
    return DB_EVENT_OK;
}

/*
 *  DB_POST_SINGLE_EVENT_PRIVATE()
 *
 *  NOTE: This assumes that the db scan lock is already applied
 */
LOCAL void db_post_single_event_private (struct evSubscrip *event)
{  
    struct event_que    *ev_que;
    db_field_log        *pLog;
    int             firstEventFlag;
    unsigned        rngSpace;

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
        return;
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
        event->nreplace++;
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
        memcpy( (char *)&pLog->field,
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
} 

/*
 *  DB_POST_EVENTS()
 *
 *  NOTE: This assumes that the db scan lock is already applied
 *
 */
int db_post_events(
void            *prec,
void            *pval,
unsigned int    select
)
{  
    struct dbCommon     *precord = (struct dbCommon *)prec;
    struct evSubscrip   *event;

    if (precord->mlis.count == 0) return DB_EVENT_OK;       /* no monitors set */

    LOCKREC(precord);
  
    for (event = (struct evSubscrip *) precord->mlis.node.next;
        event; event = (struct evSubscrip *) event->node.next){
        
        /*
         * Only send event msg if they are waiting on the field which
         * changed or pval==NULL and waiting on alarms and alarms changed
         */
        if ( (event->paddr->pfield == (void *)pval || pval==NULL) &&
            (select & event->select)) {

            db_post_single_event_private (event);
        }
    }

    UNLOCKREC(precord);
    return DB_EVENT_OK;

}

/*
 *  DB_POST_SINGLE_EVENT()
 */
void db_post_single_event (dbEventSubscription es)
{  
    struct evSubscrip *event = (struct evSubscrip *) es;
    struct dbCommon *precord = event->paddr->precord;

    dbScanLock (precord);
    db_post_single_event_private (event);
    dbScanUnlock (precord);
}

/*
 * EVENT_READ()
 */
LOCAL int event_read (struct event_que *ev_que)
{
    struct evSubscrip *event;
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
            return DB_EVENT_OK;
        }
    
    /*
     * Fetch fast register copy
     */
    for(    event=ev_que->evque[ev_que->getix];
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

    return DB_EVENT_OK;
}

/*
 * EVENT_TASK()
 */
LOCAL void event_task (void *pParm)
{
    struct event_user   *evUser = (struct event_user *) pParm;
    int                 status;
    struct event_que    *ev_que;

    /* init hook */
    if (evUser->init_func) {
        status = (*evUser->init_func)(evUser->init_func_arg);
        if (status!=DB_EVENT_OK) {
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

        for(    ev_que= &evUser->firstque; 
            ev_que; 
            ev_que = ev_que->nextque){
            event_read (ev_que);
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

    semMutexDestroy(evUser->firstque.writelock);

    /* joh- added this code to free additional event queues */
    {
            struct event_que    *nextque;

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
 * DB_START_EVENTS()
 */
int db_start_events (dbEventCtx ctx, char *taskname, int (*init_func)(void *), 
                     void *init_func_arg, int priority_offset)
{
     struct event_user *evUser = (struct event_user *) ctx;
     int     taskpri;
     
     semMutexMustTake (evUser->firstque.writelock);

     /* 
      * only one ca_pend_event thread may be 
      * started for each evUser
      */
     if (evUser->taskid) {
         semMutexGive (evUser->firstque.writelock);
         return DB_EVENT_OK;
     }

     taskpri = threadGetPriority (threadGetIdSelf());
     taskpri -= priority_offset;
     evUser->pendexit = FALSE;
     evUser->init_func = init_func;
     evUser->init_func_arg = init_func_arg;
     if (!taskname) {
         taskname = EVENT_PEND_NAME;
     }
     evUser->taskid = threadCreate (
         taskname, taskpri, threadGetStackSize(threadStackMedium),
         event_task, (void *)evUser);
     if (!evUser->taskid) {
         semMutexGive (evUser->firstque.writelock);
         return DB_EVENT_ERROR;
     }
     semMutexGive (evUser->firstque.writelock);
     return DB_EVENT_OK;
}

/*
 * db_event_flow_ctrl_mode_on()
 */
void db_event_flow_ctrl_mode_on (dbEventCtx ctx)
{
    struct event_user *evUser = (struct event_user *) ctx;

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
void db_event_flow_ctrl_mode_off (dbEventCtx ctx)
{
    struct event_user *evUser = (struct event_user *) ctx;

    evUser->flowCtrlMode = FALSE;
    /* 
     * notify the event handler task
     */
    semBinaryGive (evUser->ppendsem);
#ifdef DEBUG
    printf("fc off %lu\n", tickGet());
#endif
}


