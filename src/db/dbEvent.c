/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbEvent.c */

/* routines for scheduling events to lower priority tasks via the RT kernel */
/*
 *  Author:     Jeffrey O. Hill 
 *      Date:            4-1-89
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "epicsAssert.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "errlog.h"
#include "taskwd.h"
#include "freeList.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "caeventmask.h"
#include "db_field_log.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbLock.h"
#include "dbAccessDefs.h"
#include "dbEvent.h"

#define EVENTSPERQUE    32
#define EVENTENTRIES    4      /* the number of que entries for each event */
#define EVENTQUESIZE    (EVENTENTRIES  * EVENTSPERQUE)
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
    char                    enabled;
};

/*
 * really a ring buffer
 */
struct event_que {
    /* lock writers to the ring buffer only */
    /* readers must never slow up writers */
    epicsMutexId            writelock;
    db_field_log            valque[EVENTQUESIZE];
    struct evSubscrip       *evque[EVENTQUESIZE];
    struct event_que        *nextque;       /* in case que quota exceeded */
    struct event_user       *evUser;        /* event user parent struct */
    unsigned short          putix;
    unsigned short          getix;
    unsigned short          quota;          /* the number of assigned entries*/
    unsigned short          nDuplicates;    /* N events duplicated on this q */ 
    unsigned short          nCanceled;      /* the number of canceled entries */
};

struct event_user {
    struct event_que    firstque;       /* the first event que */
    
    epicsMutexId        lock;
    epicsEventId        ppendsem;       /* Wait while empty */
    epicsEventId        pflush_sem;     /* wait for flush */
        
    EXTRALABORFUNC      *extralabor_sub;/* off load to event task */
    void                *extralabor_arg;/* parameter to above */
    
    epicsThreadId       taskid;         /* event handler task id */
    struct evSubscrip   *pSuicideEvent; /* event that is deleteing itself */
    unsigned            queovr;         /* event que overflow count */
    unsigned char       pendexit;       /* exit pend task */
    unsigned char       extra_labor;    /* if set call extra labor func */
    unsigned char       flowCtrlMode;   /* replace existing monitor */
    unsigned char       extraLaborBusy;
    void                (*init_func)();
    epicsThreadId       init_func_arg;
};

/*
 * Reliable intertask communication requires copying the current value of the
 * channel for later queing so 3 stepper motor steps of 10 each do not turn
 * into only 10 or 20 total steps part of the time.
 */

#define RNGINC(OLD)\
( (unsigned short) ( (OLD) >= (EVENTQUESIZE-1) ? 0 : (OLD)+1 ) )

#define LOCKEVQUE(EV_QUE)\
epicsMutexMustLock((EV_QUE)->writelock);

#define UNLOCKEVQUE(EV_QUE)\
epicsMutexUnlock((EV_QUE)->writelock);

#define LOCKREC(RECPTR)\
epicsMutexMustLock((RECPTR)->mlok);

#define UNLOCKREC(RECPTR)\
epicsMutexUnlock((RECPTR)->mlok);

static void *dbevEventUserFreeList;
static void *dbevEventQueueFreeList;
static void *dbevEventBlockFreeList;

static char *EVENT_PEND_NAME = "eventTask";

static struct evSubscrip canceledEvent;

static unsigned short ringSpace ( const struct event_que *pevq ) 
{
    if ( pevq->evque[pevq->putix] == EVENTQEMPTY ) {
        if ( pevq->getix > pevq->putix ) {
            return ( unsigned short ) ( pevq->getix - pevq->putix );
        }
        else {
            return ( unsigned short ) ( ( EVENTQUESIZE + pevq->getix ) - pevq->putix ); 
        }
    }
    return 0;
}

/*
 *  db_event_list ()
 */
int epicsShareAPI db_event_list ( const char *pname, unsigned level )
{
    return dbel ( pname, level );
}

/*
 * dbel ()
 */
int epicsShareAPI dbel ( const char *pname, unsigned level )
{
    DBADDR              addr;
    long                status;
    struct evSubscrip   *pevent;
    dbFldDes 		    *pdbFldDes;

    if ( ! pname ) return DB_EVENT_OK;
    status = dbNameToAddr ( pname, &addr );
    if ( status != 0 ) {
	    errMessage ( status, " dbNameToAddr failed" );
        return DB_EVENT_ERROR;
    }

    LOCKREC ( addr.precord );

    pevent = (struct evSubscrip *) ellFirst ( &addr.precord->mlis );

    if ( ! pevent ) {
	    printf ( "\"%s\": No PV event subscriptions ( monitors ).\n", pname );
        UNLOCKREC (addr.precord);
        return DB_EVENT_OK;
    }

    printf ( "%u PV Event Subscriptions ( monitors ).\n",
        ellCount ( &addr.precord->mlis ) );

    while ( pevent ) {
	    pdbFldDes = pevent->paddr->pfldDes;

        if ( level > 0 ) {
	        printf ( "%4.4s", pdbFldDes->name );

	        printf ( " { " );
	        if ( pevent->select & DBE_VALUE ) printf( "VALUE " );
	        if ( pevent->select & DBE_LOG ) printf( "LOG " );
	        if ( pevent->select & DBE_ALARM ) printf( "ALARM " );
	        if ( pevent->select & DBE_PROPERTY ) printf( "PROPERTY " );
	        printf ( "}" );

            if ( pevent->npend ) {
                printf ( " undelivered=%ld", pevent->npend );
            }

            if ( level > 1 ) {
                unsigned nEntriesFree;
                const void * taskId;
                LOCKEVQUE(pevent->ev_que);
                nEntriesFree = ringSpace ( pevent->ev_que );
                taskId = ( void * ) pevent->ev_que->evUser->taskid;
                UNLOCKEVQUE(pevent->ev_que);
                if ( nEntriesFree == 0u ) {
                    printf ( ", thread=%p, queue full", 
                        (void *) taskId );
                }
                else if ( nEntriesFree == EVENTQUESIZE ) {
                    printf ( ", thread=%p, queue empty", 
                        (void *) taskId );
                }
                else {
                    printf ( ", thread=%p, unused entries=%u", 
                        (void *) taskId, nEntriesFree );
                }
            }

            if ( level > 2 ) {
                unsigned nDuplicates;
                unsigned nCanceled;
                if ( pevent->nreplace ) {
                    printf (", discarded by replacement=%ld", pevent->nreplace);
                }
                if ( ! pevent->valque ) {
                    printf (", queueing disabled" );
                }
                LOCKEVQUE(pevent->ev_que);
                nDuplicates = pevent->ev_que->nDuplicates;
                nCanceled = pevent->ev_que->nCanceled;
                UNLOCKEVQUE(pevent->ev_que);
                if  ( nDuplicates ) {
                    printf (", duplicate count =%u\n", nDuplicates );
                }
                if ( nCanceled ) {
                    printf (", canceled count =%u\n", nCanceled );
                }
            }

            if ( level > 3 ) {
                printf ( ", ev %p, ev que %p, ev user %p", 
                    ( void * ) pevent, 
                    ( void * ) pevent->ev_que, 
                    ( void * ) pevent->ev_que->evUser );
            }

	        printf( "\n" );
        }

	    pevent = (struct evSubscrip *) ellNext ( &pevent->node );
    }

    UNLOCKREC ( addr.precord );

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
dbEventCtx epicsShareAPI db_init_events (void)
{
    struct event_user * evUser;
    
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
    evUser->firstque.writelock = epicsMutexCreate();
    if (!evUser->firstque.writelock) {
        return NULL;
    }

    evUser->ppendsem = epicsEventCreate(epicsEventEmpty);
    if (!evUser->ppendsem) {
        epicsMutexDestroy (evUser->firstque.writelock);
        return NULL;
    }    
    evUser->pflush_sem = epicsEventCreate(epicsEventEmpty);
    if (!evUser->pflush_sem) {
        epicsMutexDestroy (evUser->firstque.writelock);
        epicsEventDestroy (evUser->ppendsem);
        return NULL;
    }
    evUser->lock = epicsMutexCreate();
    if (!evUser->lock) {
        epicsMutexDestroy (evUser->firstque.writelock);
        epicsEventDestroy (evUser->pflush_sem);
        epicsEventDestroy (evUser->ppendsem);
        return NULL;
    }

    evUser->flowCtrlMode = FALSE;
    evUser->extraLaborBusy = FALSE;
    evUser->pSuicideEvent = NULL;
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
void epicsShareAPI db_close_events (dbEventCtx ctx)
{
    struct event_user * const evUser = (struct event_user *) ctx;

    /*
     * Exit not forced on event blocks for now - this is left to channel
     * access and any other tasks using this facility which can find them
     * more efficiently.
     * 
     * NOTE: not deleting events before calling this routine could be
     * hazardous to the system's health.
     */
    epicsMutexMustLock ( evUser->lock );
    evUser->pendexit = TRUE;
    epicsMutexUnlock ( evUser->lock );
    /* notify the waiting task */
    epicsEventSignal(evUser->ppendsem);
}

/*
 * create_ev_que()
 */
static struct event_que * create_ev_que ( struct event_user * const evUser )
{
    struct event_que * const ev_que = (struct event_que *) 
        freeListCalloc ( dbevEventQueueFreeList );
    if ( ! ev_que ) {
        return NULL;
    }
    ev_que->writelock = epicsMutexCreate();
    if ( ! ev_que->writelock ) {
        freeListFree ( dbevEventQueueFreeList, ev_que );
        return NULL;
    }
    ev_que->evUser = evUser;
    return ev_que;
}

/*
 * DB_ADD_EVENT()
 */
dbEventSubscription epicsShareAPI db_add_event (
    dbEventCtx ctx, struct dbAddr *paddr,
    EVENTFUNC *user_sub, void *user_arg, unsigned select)
{
    struct event_user * const evUser = (struct event_user *) ctx;
    struct event_que * ev_que;
    struct evSubscrip * pevent;

    /*
     * Don't add events which will not be triggered
     */
    if ( select==0 || select > UCHAR_MAX ) {
        return NULL;
    }

    pevent = freeListCalloc ( dbevEventBlockFreeList );
    if ( ! pevent ) {
        return NULL;
    }

    /* find an event que block with enough quota */
    /* otherwise add a new one to the list */
    epicsMutexMustLock ( evUser->lock );
    ev_que = & evUser->firstque;
    while ( TRUE ) {
        int success = 0;
        LOCKEVQUE ( ev_que );
        success = ( ev_que->quota + ev_que->nCanceled < 
                                EVENTQUESIZE - EVENTENTRIES );
        if ( success ) {
            ev_que->quota += EVENTENTRIES;
        }
        UNLOCKEVQUE ( ev_que );
        if ( success ) {
            break;
        }
        if ( ! ev_que->nextque ) {
            ev_que->nextque = create_ev_que ( evUser );
            if ( ! ev_que->nextque ) {
                ev_que = NULL;
                break;
            }
        }
        ev_que = ev_que->nextque;
    }
    epicsMutexUnlock ( evUser->lock );

    if ( ! ev_que ) {
        freeListFree ( dbevEventBlockFreeList, pevent );
        return NULL;
    }

    pevent->npend =     0ul;
    pevent->nreplace =  0ul;
    pevent->user_sub =  user_sub;
    pevent->user_arg =  user_arg;
    pevent->paddr =     paddr;
    pevent->select =    (unsigned char) select;
    pevent->pLastLog =  NULL; /* not yet in the queue */
    pevent->callBackInProgress = FALSE;
    pevent->enabled =   FALSE;
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

    return pevent;
}

/*
 * db_event_enable()
 */
void epicsShareAPI db_event_enable (dbEventSubscription es)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) es;
    struct dbCommon * const precord = 
                (struct dbCommon *) pevent->paddr->precord;

    LOCKREC(precord);
    if ( ! pevent->enabled ) {
        ellAdd (&precord->mlis, &pevent->node);
        pevent->enabled = TRUE;
    }
    UNLOCKREC(precord);
}

/*
 * db_event_disable()
 */
void epicsShareAPI db_event_disable (dbEventSubscription es)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) es;
    struct dbCommon * const precord = 
                (struct dbCommon *) pevent->paddr->precord;

    LOCKREC(precord);
    if ( pevent->enabled ) {
        ellDelete(&precord->mlis, &pevent->node);
        pevent->enabled = FALSE;
    }
    UNLOCKREC(precord);
}

/*
 * event_remove()
 * event queue lock _must_ be applied
 */
static void event_remove ( struct event_que *ev_que, 
    unsigned short index, struct evSubscrip *placeHolder )
{
    struct evSubscrip * const pEvent = ev_que->evque[index];

    ev_que->evque[index] = placeHolder;
    if ( pEvent->npend == 1u ) {
        pEvent->pLastLog = NULL;
    }
    else {
        assert ( pEvent->npend > 1u );
        assert ( ev_que->nDuplicates >= 1u );
        ev_que->nDuplicates--;
    }
    pEvent->npend--;
}

/*
 * DB_CANCEL_EVENT()
 *
 * This routine does not prevent two threads from deleting 
 * the same block at the same time.
 * 
 */
void epicsShareAPI db_cancel_event (dbEventSubscription es)
{
    struct evSubscrip * const pevent = ( struct evSubscrip * ) es;
    unsigned short getix;

    db_event_disable ( es );

    /*
     * flag the event as canceled by NULLing out the callback handler 
     *
     * make certain that the event isnt being accessed while
     * its call back changes
     */
    LOCKEVQUE ( pevent->ev_que )

    pevent->user_sub = NULL;

    /*
     * purge this event from the queue
     *
     * Its better to take this approach rather than waiting 
     * for the event thread to finish removing this event
     * from the queue because the event thread will not
     * process if we are in flow control mode. Since blocking 
     * here will block CA's TCP input queue then a dead lock
     * would be possible.
     */
    for (   getix = pevent->ev_que->getix; 
            pevent->ev_que->evque[getix] != EVENTQEMPTY; ) {
        if ( pevent->ev_que->evque[getix] == pevent ) {
            assert ( pevent->ev_que->nCanceled < USHRT_MAX );
            pevent->ev_que->nCanceled++;
            event_remove ( pevent->ev_que, getix, &canceledEvent );
        }
        getix = RNGINC ( getix );
        if ( getix == pevent->ev_que->getix ) {
            break;
        }
    }
    assert ( pevent->npend == 0u );

    if ( pevent->ev_que->evUser->taskid == epicsThreadGetIdSelf() ) {
        pevent->ev_que->evUser->pSuicideEvent = pevent;
    }
    else {
        while ( pevent->callBackInProgress ) {
            UNLOCKEVQUE ( pevent->ev_que )
            epicsEventMustWait ( pevent->ev_que->evUser->pflush_sem );
            LOCKEVQUE ( pevent->ev_que )
        }
    }

    pevent->ev_que->quota -= EVENTENTRIES;

    UNLOCKEVQUE ( pevent->ev_que )
        
    freeListFree ( dbevEventBlockFreeList, pevent );

    return;
}

/*
 * DB_FLUSH_EXTRA_LABOR_EVENT()
 *
 * waits for extra labor in progress to finish
 */
void epicsShareAPI db_flush_extra_labor_event (dbEventCtx ctx)
{
    struct event_user * const evUser = (struct event_user *) ctx;

	epicsMutexMustLock ( evUser->lock );
    while ( evUser->extraLaborBusy ) {
        epicsMutexUnlock ( evUser->lock );
        epicsThreadSleep(0.1);
        epicsMutexMustLock ( evUser->lock );
    }
    epicsMutexUnlock ( evUser->lock );
}

/*
 * DB_ADD_EXTRA_LABOR_EVENT()
 *
 * Specify a routine to be called
 * when labor is offloaded to the
 * event task
 */
int epicsShareAPI db_add_extra_labor_event (
    dbEventCtx ctx, EXTRALABORFUNC *func, void *arg)
{
    struct event_user * const evUser = (struct event_user *) ctx;

    epicsMutexMustLock ( evUser->lock );
    evUser->extralabor_sub = func;
    evUser->extralabor_arg = arg;
    epicsMutexUnlock ( evUser->lock );

    return DB_EVENT_OK;
}

/*
 *  DB_POST_EXTRA_LABOR()
 */
int epicsShareAPI db_post_extra_labor (dbEventCtx ctx)
{
    struct event_user * const evUser = (struct event_user *) ctx;
    int doit;

    epicsMutexMustLock ( evUser->lock );
    if ( ! evUser->extra_labor ) {
        evUser->extra_labor = TRUE;
        doit = TRUE;
    }
    else {
        doit = FALSE;
    }
    epicsMutexUnlock ( evUser->lock );

    if ( doit ) {
        epicsEventSignal(evUser->ppendsem);
    }

    return DB_EVENT_OK;
}

/*
 *  DB_POST_SINGLE_EVENT_PRIVATE()
 *
 *  NOTE: This assumes that the db scan lock is already applied
 */
static void db_post_single_event_private (struct evSubscrip *event)
{  
    struct event_que * const ev_que = event->ev_que;
    db_field_log * pLog;
    int firstEventFlag;
    unsigned rngSpace;

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
    rngSpace = ringSpace ( ev_que );
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
 	 * Otherwise, the current entry must be available.
     * Fill it in and advance the ring buffer.
     */
    else {
        assert ( ev_que->evque[ev_que->putix] == EVENTQEMPTY );
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
        ev_que->putix = RNGINC ( ev_que->putix );
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
        epicsEventSignal(ev_que->evUser->ppendsem);
    }
} 

/*
 *  DB_POST_EVENTS()
 *
 *  NOTE: This assumes that the db scan lock is already applied
 *
 */
int epicsShareAPI db_post_events(
void            *pRecord,
void            *pField,
unsigned int    caEventMask
)
{  
    struct dbCommon * const pdbc = (struct dbCommon *)pRecord;
    struct evSubscrip * event;

    if (pdbc->mlis.count == 0) return DB_EVENT_OK; /* no monitors set */

    LOCKREC(pdbc);
  
    for (event = (struct evSubscrip *) pdbc->mlis.node.next;
        event; event = (struct evSubscrip *) event->node.next){
        
        /*
         * Only send event msg if they are waiting on the field which
         * changed or pval==NULL and waiting on alarms and alarms changed
         */
        if ( (event->paddr->pfield == (void *)pField || pField==NULL) &&
            (caEventMask & event->select) ) {
            db_post_single_event_private (event);
        }
    }

    UNLOCKREC(pdbc);
    return DB_EVENT_OK;

}

/*
 *  DB_POST_SINGLE_EVENT()
 */
void epicsShareAPI db_post_single_event (dbEventSubscription es)
{  
    struct evSubscrip * const event = (struct evSubscrip *) es;
    struct dbCommon * const precord = event->paddr->precord;

    dbScanLock (precord);
    db_post_single_event_private (event);
    dbScanUnlock (precord);
}

/*
 * EVENT_READ()
 */
static int event_read ( struct event_que *ev_que )
{
    db_field_log *pfl;
    void ( *user_sub ) ( void *user_arg, struct dbAddr *paddr, 
            int eventsRemaining, db_field_log *pfl );
    
    /*
     * evUser ring buffer must be locked for the multiple
     * threads writing/reading it
     */
    LOCKEVQUE ( ev_que )
        
    /*
     * if in flow control mode drain duplicates and then
     * suspend processing events until flow control
     * mode is over
     */
    if ( ev_que->evUser->flowCtrlMode && ev_que->nDuplicates == 0u ) {
        UNLOCKEVQUE(ev_que);
        return DB_EVENT_OK;
    }
    
    while ( ev_que->evque[ev_que->getix] != EVENTQEMPTY ) {
        db_field_log fl = ev_que->valque[ev_que->getix];
        struct evSubscrip *event = ev_que->evque[ev_que->getix];

        if ( event == &canceledEvent ) {
            ev_que->evque[ev_que->getix] = EVENTQEMPTY;
            ev_que->getix = RNGINC ( ev_que->getix );
            assert ( ev_que->nCanceled > 0 );
            ev_que->nCanceled--;
            continue;
        }

        /*
         * Simple type values queued up for reliable interprocess
         * communication. (for other types they get whatever happens
         * to be there upon wakeup)
         */
        if ( event->valque ) {
            pfl = &fl;
        }
        else {
            pfl = NULL;
        }

        event_remove ( ev_que, ev_que->getix, EVENTQEMPTY );
        ev_que->getix = RNGINC ( ev_que->getix );

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
        if ( user_sub ) {
            /*
             * This provides a way to test to see if an event is in use
             * despite the fact that the event queue does not point to 
             * it. 
             */
            event->callBackInProgress = TRUE;
            UNLOCKEVQUE ( ev_que )
            ( *user_sub ) ( event->user_arg, event->paddr, 
                ev_que->evque[ev_que->getix] != EVENTQEMPTY, pfl );
            LOCKEVQUE ( ev_que )

            /*
             * check to see if this event has been canceled each
             * time that the callBackInProgress flag is set to false
             * while we have the event queue lock, and post the flush
             * complete sem if there are no longer any events on the
             * queue
             */
            if ( ev_que->evUser->pSuicideEvent == event ) {
                ev_que->evUser->pSuicideEvent = NULL;
            }
            else {
                if ( event->user_sub==NULL && event->npend==0u ) {
                    event->callBackInProgress = FALSE;
                    epicsEventSignal ( ev_que->evUser->pflush_sem );
                }
                else {
                    event->callBackInProgress = FALSE;
                }
            }
        }
    }

    UNLOCKEVQUE ( ev_que )

    return DB_EVENT_OK;
}

/*
 * EVENT_TASK()
 */
static void event_task (void *pParm)
{
    struct event_user * const evUser = (struct event_user *) pParm;
    struct event_que * ev_que;
    unsigned char pendexit;

    /* init hook */
    if (evUser->init_func) {
        (*evUser->init_func)(evUser->init_func_arg);
    }

    taskwdInsert ( epicsThreadGetIdSelf(), NULL, NULL );

    do{
        void (*pExtraLaborSub) (void *);
        void *pExtraLaborArg;
        epicsEventMustWait(evUser->ppendsem);

        /*
         * check to see if the caller has offloaded
         * labor to this task
         */
        epicsMutexMustLock ( evUser->lock );
        evUser->extraLaborBusy = TRUE;
        if ( evUser->extra_labor && evUser->extralabor_sub ) {
            evUser->extra_labor = FALSE;
            pExtraLaborSub = evUser->extralabor_sub;
            pExtraLaborArg = evUser->extralabor_arg;
        }
        else {
            pExtraLaborSub = NULL;
            pExtraLaborArg = NULL;
        }
        if ( pExtraLaborSub ) {
            epicsMutexUnlock ( evUser->lock );
            (*pExtraLaborSub)(pExtraLaborArg);
            epicsMutexMustLock ( evUser->lock );
        }
        evUser->extraLaborBusy = FALSE;

        for ( ev_que = &evUser->firstque; ev_que; 
                ev_que = ev_que->nextque ) {
            epicsMutexUnlock ( evUser->lock );
            event_read (ev_que);
            epicsMutexMustLock ( evUser->lock );
        }
        pendexit = evUser->pendexit;
        epicsMutexUnlock ( evUser->lock );

    } while( ! pendexit );

    epicsMutexDestroy(evUser->firstque.writelock);

    {
        struct event_que    *nextque;

        ev_que = evUser->firstque.nextque;
        while(ev_que){ 
            nextque = ev_que->nextque;
            epicsMutexDestroy(ev_que->writelock);
            freeListFree(dbevEventQueueFreeList, ev_que);
            ev_que = nextque;
        }
    }

    epicsEventDestroy(evUser->ppendsem);
    epicsEventDestroy(evUser->pflush_sem);
    epicsMutexDestroy(evUser->lock);

    freeListFree(dbevEventUserFreeList, evUser);

    taskwdRemove(epicsThreadGetIdSelf());

    return;
}

/*
 * DB_START_EVENTS()
 */
int epicsShareAPI db_start_events (
    dbEventCtx ctx,const char *taskname, void (*init_func)(void *), 
    void *init_func_arg, unsigned osiPriority )
{
     struct event_user * const evUser = (struct event_user *) ctx;
     
     epicsMutexMustLock ( evUser->lock );

     /* 
      * only one ca_pend_event thread may be 
      * started for each evUser
      */
     if (evUser->taskid) {
         epicsMutexUnlock ( evUser->lock );
         return DB_EVENT_OK;
     }

     evUser->pendexit = FALSE;
     evUser->init_func = init_func;
     evUser->init_func_arg = init_func_arg;
     if (!taskname) {
         taskname = EVENT_PEND_NAME;
     }
     evUser->taskid = epicsThreadCreate (
         taskname, osiPriority, 
         epicsThreadGetStackSize(epicsThreadStackMedium),
         event_task, (void *)evUser);
     if (!evUser->taskid) {
         epicsMutexUnlock ( evUser->lock );
         return DB_EVENT_ERROR;
     }
     epicsMutexUnlock ( evUser->lock );
     return DB_EVENT_OK;
}

/*
 * db_event_change_priority()
 */
void epicsShareAPI db_event_change_priority ( dbEventCtx ctx, 
                                        unsigned epicsPriority )
{
    struct event_user * const evUser = ( struct event_user * ) ctx;
    epicsThreadSetPriority ( evUser->taskid, epicsPriority );
}

/*
 * db_event_flow_ctrl_mode_on()
 */
void epicsShareAPI db_event_flow_ctrl_mode_on (dbEventCtx ctx)
{
    struct event_user * const evUser = (struct event_user *) ctx;

    epicsMutexMustLock ( evUser->lock );
    evUser->flowCtrlMode = TRUE;
    epicsMutexUnlock ( evUser->lock );
    /* 
     * notify the event handler task
     */
    epicsEventSignal(evUser->ppendsem);
#ifdef DEBUG
    printf("fc on %lu\n", tickGet());
#endif
}

/*
 * db_event_flow_ctrl_mode_off()
 */
void epicsShareAPI db_event_flow_ctrl_mode_off (dbEventCtx ctx)
{
    struct event_user * const evUser = (struct event_user *) ctx;

    epicsMutexMustLock ( evUser->lock );
    evUser->flowCtrlMode = FALSE;
    epicsMutexUnlock ( evUser->lock );
    /* 
     * notify the event handler task
     */
    epicsEventSignal (evUser->ppendsem);
#ifdef DEBUG
    printf("fc off %lu\n", tickGet());
#endif
} 


