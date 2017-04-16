/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbEvent.c */

/*
 *  Author: Jeffrey O. Hill <johill@lanl.gov>
 *
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cantProceed.h"
#include "dbDefs.h"
#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "errlog.h"
#include "freeList.h"
#include "taskwd.h"

#include "caeventmask.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "link.h"
#include "special.h"

#define EVENTSPERQUE    32
#define EVENTENTRIES    4      /* the number of que entries for each event */
#define EVENTQUESIZE    (EVENTENTRIES  * EVENTSPERQUE)
#define EVENTQEMPTY     ((struct evSubscrip *)NULL)

/*
 * really a ring buffer
 */
struct event_que {
    /* lock writers to the ring buffer only */
    /* readers must never slow up writers */
    epicsMutexId            writelock;
    db_field_log            *valque[EVENTQUESIZE];
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
    epicsEventId        pexitsem;       /* wait for event task to join */

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

#define LOCKEVQUE(EV_QUE)   epicsMutexMustLock((EV_QUE)->writelock)
#define UNLOCKEVQUE(EV_QUE) epicsMutexUnlock((EV_QUE)->writelock)
#define LOCKREC(RECPTR)     epicsMutexMustLock((RECPTR)->mlok)
#define UNLOCKREC(RECPTR)   epicsMutexUnlock((RECPTR)->mlok)

static void *dbevEventUserFreeList;
static void *dbevEventQueueFreeList;
static void *dbevEventSubscriptionFreeList;
static void *dbevFieldLogFreeList;

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
int db_event_list ( const char *pname, unsigned level )
{
    return dbel ( pname, level );
}

/*
 * dbel ()
 */
int dbel ( const char *pname, unsigned level )
{
    DBADDR              addr;
    long                status;
    struct evSubscrip   *pevent;
    dbFldDes            *pdbFldDes;

    if ( ! pname ) return DB_EVENT_OK;
    status = dbNameToAddr ( pname, &addr );
    if ( status != 0 ) {
	    errMessage ( status, " dbNameToAddr failed" );
        return DB_EVENT_ERROR;
    }

    LOCKREC (addr.precord);

    pevent = (struct evSubscrip *) ellFirst ( &addr.precord->mlis );

    if ( ! pevent ) {
	    printf ( "\"%s\": No PV event subscriptions ( monitors ).\n", pname );
        UNLOCKREC (addr.precord);
        return DB_EVENT_OK;
    }

    printf ( "%u PV Event Subscriptions ( monitors ).\n",
        ellCount ( &addr.precord->mlis ) );

    while ( pevent ) {
        pdbFldDes = dbChannelFldDes(pevent->chan);

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
                if ( ! pevent->useValque ) {
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
    struct event_user * evUser;

    if (!dbevEventUserFreeList) {
        freeListInitPvt(&dbevEventUserFreeList,
            sizeof(struct event_user),8);
    }
    if (!dbevEventQueueFreeList) {
        freeListInitPvt(&dbevEventQueueFreeList,
            sizeof(struct event_que),8);
    }
    if (!dbevEventSubscriptionFreeList) {
        freeListInitPvt(&dbevEventSubscriptionFreeList,
            sizeof(struct evSubscrip),256);
    }
    if (!dbevFieldLogFreeList) {
        freeListInitPvt(&dbevFieldLogFreeList,
            sizeof(struct db_field_log),2048);
    }

    evUser = (struct event_user *)
        freeListCalloc(dbevEventUserFreeList);
    if (!evUser) {
        return NULL;
    }

    /* Flag will be cleared when event task starts */
    evUser->pendexit = TRUE;

    evUser->firstque.evUser = evUser;
    evUser->firstque.writelock = epicsMutexCreate();
    if (!evUser->firstque.writelock)
        goto fail;

    evUser->ppendsem = epicsEventCreate(epicsEventEmpty);
    if (!evUser->ppendsem)
        goto fail;
    evUser->pflush_sem = epicsEventCreate(epicsEventEmpty);
    if (!evUser->pflush_sem)
        goto fail;
    evUser->lock = epicsMutexCreate();
    if (!evUser->lock)
        goto fail;
    evUser->pexitsem = epicsEventCreate(epicsEventEmpty);
    if (!evUser->pexitsem)
        goto fail;

    evUser->flowCtrlMode = FALSE;
    evUser->extraLaborBusy = FALSE;
    evUser->pSuicideEvent = NULL;
    return (dbEventCtx) evUser;
fail:
    if(evUser->lock)
        epicsMutexDestroy (evUser->lock);
    if(evUser->firstque.writelock)
        epicsMutexDestroy (evUser->firstque.writelock);
    if(evUser->ppendsem)
        epicsEventDestroy (evUser->ppendsem);
    if(evUser->pflush_sem)
        epicsEventDestroy (evUser->pflush_sem);
    if(evUser->pexitsem)
        epicsEventDestroy (evUser->pexitsem);
    freeListFree(dbevEventUserFreeList,evUser);
    return NULL;
}


epicsShareFunc void db_cleanup_events(void)
{
    freeListCleanup(dbevEventUserFreeList);
    dbevEventUserFreeList = NULL;

    freeListCleanup(dbevEventQueueFreeList);
    dbevEventQueueFreeList = NULL;

    freeListCleanup(dbevEventSubscriptionFreeList);
    dbevEventSubscriptionFreeList = NULL;

    freeListCleanup(dbevFieldLogFreeList);
    dbevFieldLogFreeList = NULL;
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
    if(!evUser->pendexit) { /* event task running */
        evUser->pendexit = TRUE;
        epicsMutexUnlock ( evUser->lock );

        /* notify the waiting task */
        epicsEventSignal(evUser->ppendsem);
        /* wait for task to exit */
        epicsEventMustWait(evUser->pexitsem);

        epicsMutexMustLock ( evUser->lock );
    }

    epicsMutexUnlock ( evUser->lock );

    epicsEventDestroy(evUser->pexitsem);
    epicsEventDestroy(evUser->ppendsem);
    epicsEventDestroy(evUser->pflush_sem);
    epicsMutexDestroy(evUser->lock);

    freeListFree(dbevEventUserFreeList, evUser);
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
dbEventSubscription db_add_event (
    dbEventCtx ctx, struct dbChannel *chan,
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

    pevent = freeListCalloc (dbevEventSubscriptionFreeList);
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
        freeListFree ( dbevEventSubscriptionFreeList, pevent );
        return NULL;
    }

    pevent->npend =     0ul;
    pevent->nreplace =  0ul;
    pevent->user_sub =  user_sub;
    pevent->user_arg =  user_arg;
    pevent->chan =      chan;
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
    if (dbChannelElements(chan) == 1 &&
        dbChannelSpecial(chan) != SPC_DBADDR &&
        dbChannelFieldSize(chan) <= sizeof(union native_value)) {
        pevent->useValque = TRUE;
    }
    else {
        pevent->useValque = FALSE;
    }

    return pevent;
}

/*
 * db_event_enable()
 */
void db_event_enable (dbEventSubscription event)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) event;
    struct dbCommon * const precord = dbChannelRecord(pevent->chan);

    LOCKREC (precord);
    if ( ! pevent->enabled ) {
        ellAdd (&precord->mlis, &pevent->node);
        pevent->enabled = TRUE;
    }
    UNLOCKREC (precord);
}

/*
 * db_event_disable()
 */
void db_event_disable (dbEventSubscription event)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) event;
    struct dbCommon * const precord = dbChannelRecord(pevent->chan);

    LOCKREC (precord);
    if ( pevent->enabled ) {
        ellDelete(&precord->mlis, &pevent->node);
        pevent->enabled = FALSE;
    }
    UNLOCKREC (precord);
}

/*
 * event_remove()
 * event queue lock _must_ be applied
 * this nulls the entry in the queue, but doesn't delete the db_field_log chunk
 */
static void event_remove ( struct event_que *ev_que,
    unsigned short index, struct evSubscrip *placeHolder )
{
    struct evSubscrip * const pevent = ev_que->evque[index];

    ev_que->evque[index] = placeHolder;
    ev_que->valque[index] = NULL;
    if ( pevent->npend == 1u ) {
        pevent->pLastLog = NULL;
    }
    else {
        assert ( pevent->npend > 1u );
        assert ( ev_que->nDuplicates >= 1u );
        ev_que->nDuplicates--;
    }
    pevent->npend--;
}

/*
 * DB_CANCEL_EVENT()
 *
 * This routine does not prevent two threads from deleting
 * the same block at the same time.
 *
 */
void db_cancel_event (dbEventSubscription event)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) event;
    unsigned short getix;

    db_event_disable ( event );

    /*
     * flag the event as canceled by NULLing out the callback handler
     *
     * make certain that the event isnt being accessed while
     * its call back changes
     */
    LOCKEVQUE (pevent->ev_que);

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
            UNLOCKEVQUE (pevent->ev_que);
            epicsEventMustWait ( pevent->ev_que->evUser->pflush_sem );
            LOCKEVQUE (pevent->ev_que);
        }
    }

    pevent->ev_que->quota -= EVENTENTRIES;

    UNLOCKEVQUE (pevent->ev_que);

    freeListFree ( dbevEventSubscriptionFreeList, pevent );

    return;
}

/*
 * DB_FLUSH_EXTRA_LABOR_EVENT()
 *
 * waits for extra labor in progress to finish
 */
void db_flush_extra_labor_event (dbEventCtx ctx)
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
int db_add_extra_labor_event (
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
int db_post_extra_labor (dbEventCtx ctx)
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
 *  DB_CREATE_EVENT_LOG()
 *
 *  NOTE: This assumes that the db scan lock is already applied
 *        (as it copies data from the record)
 */
db_field_log* db_create_event_log (struct evSubscrip *pevent)
{
    db_field_log *pLog = (db_field_log *) freeListCalloc(dbevFieldLogFreeList);

    if (pLog) {
        struct dbChannel *chan = pevent->chan;
        struct dbCommon  *prec = dbChannelRecord(chan);
        pLog->ctx = dbfl_context_event;
        if (pevent->useValque) {
            pLog->type = dbfl_type_val;
            pLog->stat = prec->stat;
            pLog->sevr = prec->sevr;
            pLog->time = prec->time;
            pLog->field_type  = dbChannelFieldType(chan);
            pLog->no_elements = dbChannelElements(chan);
            /*
             * use memcpy to avoid a bus error on
             * union copy of char in the db at an odd
             * address
             */
            memcpy(&pLog->u.v.field,
                   dbChannelField(chan),
                   dbChannelFieldSize(chan));
        } else {
            pLog->type = dbfl_type_rec;
        }
    }
    return pLog;
}

/*
 *  DB_CREATE_READ_LOG()
 *
 */
db_field_log* db_create_read_log (struct dbChannel *chan)
{
    db_field_log *pLog = (db_field_log *) freeListCalloc(dbevFieldLogFreeList);

    if (pLog) {
        pLog->ctx  = dbfl_context_read;
        pLog->type = dbfl_type_rec;
    }
    return pLog;
}

/*
 *  DB_QUEUE_EVENT_LOG()
 *
 */
static void db_queue_event_log (evSubscrip *pevent, db_field_log *pLog)
{
    struct event_que    *ev_que;
    int firstEventFlag;
    unsigned rngSpace;

    ev_que = pevent->ev_que;
    /*
     * evUser ring buffer must be locked for the multiple
     * threads writing/reading it
     */

    LOCKEVQUE (ev_que);

    /*
     * if we have an event on the queue and both the last
     * event on the queue and the current event are emtpy
     * (i.e. of type dbfl_type_rec), simply ignore duplicate
     * events (saving empty events serves no purpose)
     */
    if (pevent->npend > 0u &&
        (*pevent->pLastLog)->type == dbfl_type_rec &&
        pLog->type == dbfl_type_rec) {
        db_delete_field_log(pLog);
        UNLOCKEVQUE (ev_que);
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
    if ( pevent->npend>0u &&
        (ev_que->evUser->flowCtrlMode || rngSpace<=EVENTSPERQUE) ) {
        /*
         * replace last event if no space is left
         */
        if (*pevent->pLastLog) {
            db_delete_field_log(*pevent->pLastLog);
            *pevent->pLastLog = pLog;
        }
        pevent->nreplace++;
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
        ev_que->evque[ev_que->putix] = pevent;
        ev_que->valque[ev_que->putix] = pLog;
        pevent->pLastLog = &ev_que->valque[ev_que->putix];
        if (pevent->npend>0u) {
            ev_que->nDuplicates++;
        }
        pevent->npend++;
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

    UNLOCKEVQUE (ev_que);

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
int db_post_events(
void            *pRecord,
void            *pField,
unsigned int    caEventMask
)
{
    struct dbCommon   * const prec = (struct dbCommon *) pRecord;
    struct evSubscrip *pevent;

    if (prec->mlis.count == 0) return DB_EVENT_OK;       /* no monitors set */

    LOCKREC (prec);

    for (pevent = (struct evSubscrip *) prec->mlis.node.next;
        pevent; pevent = (struct evSubscrip *) pevent->node.next){

        /*
         * Only send event msg if they are waiting on the field which
         * changed or pval==NULL, and are waiting on matching event
         */
        if ( (dbChannelField(pevent->chan) == (void *)pField || pField==NULL) &&
            (caEventMask & pevent->select)) {
            db_field_log *pLog = db_create_event_log(pevent);
            pLog = dbChannelRunPreChain(pevent->chan, pLog);
            if (pLog) db_queue_event_log(pevent, pLog);
        }
    }

    UNLOCKREC (prec);
    return DB_EVENT_OK;

}

/*
 *  DB_POST_SINGLE_EVENT()
 */
void db_post_single_event (dbEventSubscription event)
{
    struct evSubscrip * const pevent = (struct evSubscrip *) event;
    struct dbCommon * const prec = dbChannelRecord(pevent->chan);
    db_field_log *pLog;

    dbScanLock (prec);

    pLog = db_create_event_log(pevent);
    pLog = dbChannelRunPreChain(pevent->chan, pLog);
    if(pLog) db_queue_event_log(pevent, pLog);

    dbScanUnlock (prec);
}

/*
 * EVENT_READ()
 */
static int event_read ( struct event_que *ev_que )
{
    db_field_log *pfl;
    void ( *user_sub ) ( void *user_arg, struct dbChannel *chan,
            int eventsRemaining, db_field_log *pfl );

    /*
     * evUser ring buffer must be locked for the multiple
     * threads writing/reading it
     */
    LOCKEVQUE (ev_que);

    /*
     * if in flow control mode drain duplicates and then
     * suspend processing events until flow control
     * mode is over
     */
    if ( ev_que->evUser->flowCtrlMode && ev_que->nDuplicates == 0u ) {
        UNLOCKEVQUE (ev_que);
        return DB_EVENT_OK;
    }

    while ( ev_que->evque[ev_que->getix] != EVENTQEMPTY ) {
        struct evSubscrip *pevent = ev_que->evque[ev_que->getix];

        pfl = ev_que->valque[ev_que->getix];
        if ( pevent == &canceledEvent ) {
            ev_que->evque[ev_que->getix] = EVENTQEMPTY;
            if (ev_que->valque[ev_que->getix]) {
                db_delete_field_log(ev_que->valque[ev_que->getix]);
                ev_que->valque[ev_que->getix] = NULL;
            }
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

        event_remove ( ev_que, ev_que->getix, EVENTQEMPTY );
        ev_que->getix = RNGINC ( ev_que->getix );

        /*
         * create a local copy of the call back parameters while
         * we still have the lock
         */
        user_sub = pevent->user_sub;

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
            pevent->callBackInProgress = TRUE;
            UNLOCKEVQUE (ev_que);
            /* Run post-event-queue filter chain */
            if (ellCount(&pevent->chan->post_chain)) {
                pfl = dbChannelRunPostChain(pevent->chan, pfl);
            }
            if (pfl) {
                /* Issue user callback */
                ( *user_sub ) ( pevent->user_arg, pevent->chan,
                                ev_que->evque[ev_que->getix] != EVENTQEMPTY, pfl );
            }
            LOCKEVQUE (ev_que);

            /*
             * check to see if this event has been canceled each
             * time that the callBackInProgress flag is set to false
             * while we have the event queue lock, and post the flush
             * complete sem if there are no longer any events on the
             * queue
             */
            if ( ev_que->evUser->pSuicideEvent == pevent ) {
                ev_que->evUser->pSuicideEvent = NULL;
            }
            else {
                if ( pevent->user_sub==NULL && pevent->npend==0u ) {
                    pevent->callBackInProgress = FALSE;
                    epicsEventSignal ( ev_que->evUser->pflush_sem );
                }
                else {
                    pevent->callBackInProgress = FALSE;
                }
            }
        }
        db_delete_field_log(pfl);
    }

    UNLOCKEVQUE (ev_que);

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

    do {
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
        while (ev_que) {
            nextque = ev_que->nextque;
            epicsMutexDestroy(ev_que->writelock);
            freeListFree(dbevEventQueueFreeList, ev_que);
            ev_que = nextque;
        }
    }

    taskwdRemove(epicsThreadGetIdSelf());

    epicsEventSignal(evUser->pexitsem);

    return;
}

/*
 * DB_START_EVENTS()
 */
int db_start_events (
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
     evUser->pendexit = FALSE;
     epicsMutexUnlock ( evUser->lock );
     return DB_EVENT_OK;
}

/*
 * db_event_change_priority()
 */
void db_event_change_priority ( dbEventCtx ctx, 
                                        unsigned epicsPriority )
{
    struct event_user * const evUser = ( struct event_user * ) ctx;
    epicsThreadSetPriority ( evUser->taskid, epicsPriority );
}

/*
 * db_event_flow_ctrl_mode_on()
 */
void db_event_flow_ctrl_mode_on (dbEventCtx ctx)
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
void db_event_flow_ctrl_mode_off (dbEventCtx ctx)
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

/*
 * db_delete_field_log()
 */
void db_delete_field_log (db_field_log *pfl)
{
    if (pfl) {
        /* Free field if reference type field log and dtor is set */
        if (pfl->type == dbfl_type_ref && pfl->u.r.dtor) pfl->u.r.dtor(pfl);
        /* Free the field log chunk */
        freeListFree(dbevFieldLogFreeList, pfl);
    }
}
