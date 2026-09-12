/*************************************************************************\
* Copyright (c) 2026 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Tests for the dbEvent subsystem.
 *
 * Tests 1-3 and 5-6 use iocBuildIsolated() with db_init_events() but
 * without db_start_events(), so the event task never drains the queue.
 * This allows deterministic inspection of queue state.
 *
 * Tests 4, 7 and 8 start the event task to test callback delivery,
 * flow control, and the extra labor mechanism.
 */

#define EPICS_PRIVATE_API
#define USE_TYPED_DBEVENT

#include <string.h>
#include <limits.h>

#include "alarm.h"
#include "dbAccess.h"
#include "dbChannel.h"
#include "dbEvent.h"
#include "dbLock.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "iocInit.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#include "osiFileName.h"

#include "xRecord.h"

/* These must match the values in dbEvent.c */
#define EVENTENTRIES    4
#define EVENTSPERQUE    36
#define EVENTQUESIZE    (EVENTENTRIES * EVENTSPERQUE)   /* 144 */

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void noop_cb(void *user_arg, struct dbChannel *chan,
                    int eventsRemaining, struct db_field_log *pfl)
{
    (void)user_arg;
    (void)chan;
    (void)eventsRemaining;
    (void)pfl;
}

/* Callback context for tests that observe callback invocation */
struct cbContext {
    epicsMutexId lock;
    epicsEventId wake;
    unsigned count;
    int lastEventsRemaining;
};

static void counting_cb(void *user_arg, struct dbChannel *chan,
                         int eventsRemaining, struct db_field_log *pfl)
{
    struct cbContext *ctx = (struct cbContext *)user_arg;
    (void)chan;
    (void)pfl;
    epicsMutexMustLock(ctx->lock);
    ctx->count++;
    ctx->lastEventsRemaining = eventsRemaining;
    epicsMutexUnlock(ctx->lock);
    epicsEventSignal(ctx->wake);
}

static void initCbContext(struct cbContext *ctx)
{
    ctx->lock = epicsMutexMustCreate();
    ctx->wake = epicsEventMustCreate(epicsEventEmpty);
    ctx->count = 0;
    ctx->lastEventsRemaining = -1;
}

static void cleanCbContext(struct cbContext *ctx)
{
    epicsEventDestroy(ctx->wake);
    epicsMutexDestroy(ctx->lock);
}

/* Helper: synchronize with event task by doing a dummy extra-labor flush.
 * After return, all events posted before this call have been delivered.
 */
static void dummylabor(void *unused)
{
    (void)unused;
}

static void syncEventTask(dbEventCtx evtCtx)
{
    db_add_extra_labor_event(evtCtx, dummylabor, NULL);
    db_post_extra_labor(evtCtx);
    db_flush_extra_labor_event(evtCtx);
    db_add_extra_labor_event(evtCtx, NULL, NULL);
}

/* Extra labor callback for test 8 */
static volatile int laborCount;

static void laborFunc(void *arg)
{
    volatile int *p = (volatile int *)arg;
    (*p)++;
}

/*
 * Test 1: Single subscription replacement threshold
 */
static void testSingleSubscription(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;
    unsigned long i;

    testDiag("---- single subscription ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chan = dbChannelCreate("x.VAL");
    if (!chan)
        testAbort("dbChannelCreate(\"x.VAL\") failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    sub = db_add_event(ctx, chan, noop_cb, NULL, DBE_VALUE);
    if (!sub)
        testAbort("db_add_event failed");
    db_event_enable(sub);

    testOk(sub->npend == 0, "npend initially 0 (got %lu)", sub->npend);
    testOk(sub->nreplace == 0, "nreplace initially 0 (got %lu)", sub->nreplace);

    for (i = 0; i < EVENTQUESIZE; i++)
        db_post_single_event(sub);

    testOk(sub->nreplace == 1,
        "single sub: nreplace == 1 (got %lu)", sub->nreplace);
    testOk(sub->npend == EVENTQUESIZE - 1,
        "single sub: npend == %d (got %lu)", EVENTQUESIZE - 1, sub->npend);

    /*
     * Cleanup is intentionally skipped here.
     * The subscriptions have pending events that would be deferred to
     * the event task for cleanup, but no event task is running.
     * The event context is leaked; the test process exits immediately.
     */
}

/*
 * Test 2: Multiple subscriptions sharing one queue
 */
static void testMultiSubscription(void)
{
#define NSUBS 4
    dbEventCtx ctx;
    dbChannel *chan[NSUBS];
    dbEventSubscription sub[NSUBS];
    int i;
    unsigned long total_npend, total_nreplace;

    testDiag("---- %d subscriptions sharing one queue ----", NSUBS);

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    for (i = 0; i < NSUBS; i++) {
        chan[i] = dbChannelCreate("x.VAL");
        if (!chan[i])
            testAbort("dbChannelCreate failed for sub %d", i);
        if (dbChannelOpen(chan[i]))
            testAbort("dbChannelOpen failed for sub %d", i);

        sub[i] = db_add_event(ctx, chan[i], noop_cb, NULL, DBE_VALUE);
        if (!sub[i])
            testAbort("db_add_event failed for sub %d", i);
        db_event_enable(sub[i]);
    }

    for (i = 0; i < EVENTQUESIZE; i++)
        db_post_single_event(sub[i % NSUBS]);

    total_npend = 0;
    total_nreplace = 0;
    for (i = 0; i < NSUBS; i++) {
        total_npend += sub[i]->npend;
        total_nreplace += sub[i]->nreplace;
    }

    testOk(total_npend + total_nreplace == EVENTQUESIZE,
        "multi sub: npend+nreplace == %d (got %lu)",
        EVENTQUESIZE, total_npend + total_nreplace);
    testOk(total_nreplace == NSUBS,
        "multi sub: total nreplace == %d (got %lu)", NSUBS, total_nreplace);
    testOk(total_npend == EVENTQUESIZE - NSUBS,
        "multi sub: total npend == %d (got %lu)",
        EVENTQUESIZE - NSUBS, total_npend);

    /* Cleanup skipped */
#undef NSUBS
}

/*
 * Test 3: Duplicate reference events must preserve new metadata
 */
static void testRefEventMetadata(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;
    struct dbCommon *prec;
    db_field_log *pfl;

    testDiag("---- reference event metadata update ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chan = dbChannelCreate("x.DESC");
    if (!chan)
        testAbort("dbChannelCreate(\"x.DESC\") failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    sub = db_add_event(ctx, chan, noop_cb, NULL, DBE_VALUE | DBE_ALARM);
    if (!sub)
        testAbort("db_add_event failed");
    db_event_enable(sub);

    testOk(!sub->useValque, "DESC subscription uses reference-type logs");

    prec = dbChannelRecord(chan);

    dbScanLock(prec);
    prec->stat = 0;
    prec->sevr = 0;
    dbScanUnlock(prec);
    db_post_single_event(sub);

    testOk(sub->npend == 1, "one event pending after first post (got %lu)",
        sub->npend);

    dbScanLock(prec);
    prec->stat = HIHI_ALARM;
    prec->sevr = MAJOR_ALARM;
    dbScanUnlock(prec);

    db_post_single_event(sub);

    testOk(sub->npend == 1,
        "still one event pending after duplicate (got %lu)", sub->npend);

    pfl = *sub->pLastLog;
    testOk(pfl->sevr == MAJOR_ALARM,
        "queued event has new severity %d (expected %d)",
        pfl->sevr, MAJOR_ALARM);
    testOk(pfl->stat == HIHI_ALARM,
        "queued event has new alarm status %d (expected %d)",
        pfl->stat, HIHI_ALARM);

    /* Cleanup skipped */
}

/*
 * Test 4: Callback delivery via the event task
 */
static void testCallbackDelivery(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;
    struct cbContext cb;
    unsigned count;

    testDiag("---- callback delivery ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");
    if (db_start_events(ctx, "evtTest4", NULL, NULL,
            epicsThreadPriorityCAServerLow) != DB_EVENT_OK)
        testAbort("db_start_events failed");

    chan = dbChannelCreate("x.VAL");
    if (!chan)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    initCbContext(&cb);
    sub = db_add_event(ctx, chan, counting_cb, &cb, DBE_VALUE);
    if (!sub)
        testAbort("db_add_event failed");
    db_event_enable(sub);

    db_post_single_event(sub);
    syncEventTask(ctx);

    epicsMutexMustLock(cb.lock);
    count = cb.count;
    epicsMutexUnlock(cb.lock);
    testOk(count == 1, "callback invoked once (got %u)", count);
    testOk(sub->npend == 0, "npend == 0 after delivery (got %lu)", sub->npend);

    db_post_single_event(sub);
    db_post_single_event(sub);
    syncEventTask(ctx);

    epicsMutexMustLock(cb.lock);
    count = cb.count;
    epicsMutexUnlock(cb.lock);
    testOk(count >= 2, "callback invoked again after rapid posts (got %u)", count);
    testOk(sub->npend == 0, "npend == 0 after drain (got %lu)", sub->npend);

    db_event_disable(sub);
    db_cancel_event(sub);
    dbChannelDelete(chan);
    cleanCbContext(&cb);
    db_close_events(ctx);
}

/*
 * Test 5: db_post_events() mask filtering
 */
static void testDbPostEventsFiltering(void)
{
    dbEventCtx ctx;
    dbChannel *chanVal, *chanAlarm;
    dbEventSubscription subVal, subAlarm;
    struct dbCommon *prec;
    DBADDR addr;

    testDiag("---- db_post_events mask filtering ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chanVal = dbChannelCreate("x.VAL");
    chanAlarm = dbChannelCreate("x.VAL");
    if (!chanVal || !chanAlarm)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chanVal) || dbChannelOpen(chanAlarm))
        testAbort("dbChannelOpen failed");

    subVal = db_add_event(ctx, chanVal, noop_cb, NULL, DBE_VALUE);
    subAlarm = db_add_event(ctx, chanAlarm, noop_cb, NULL, DBE_ALARM);
    if (!subVal || !subAlarm)
        testAbort("db_add_event failed");
    db_event_enable(subVal);
    db_event_enable(subAlarm);

    prec = dbChannelRecord(chanVal);
    dbNameToAddr("x.VAL", &addr);

    /* Post DBE_VALUE only */
    dbScanLock(prec);
    db_post_events(prec, addr.pfield, DBE_VALUE);
    dbScanUnlock(prec);

    testOk(subVal->npend == 1,
        "DBE_VALUE sub got event (npend %lu)", subVal->npend);
    testOk(subAlarm->npend == 0,
        "DBE_ALARM sub skipped (npend %lu)", subAlarm->npend);

    /* Post DBE_ALARM only */
    dbScanLock(prec);
    db_post_events(prec, addr.pfield, DBE_ALARM);
    dbScanUnlock(prec);

    testOk(subVal->npend == 1,
        "DBE_VALUE sub still 1 (npend %lu)", subVal->npend);
    testOk(subAlarm->npend == 1,
        "DBE_ALARM sub got event (npend %lu)", subAlarm->npend);

    /* pField==NULL broadcasts to all matching */
    dbScanLock(prec);
    db_post_events(prec, NULL, DBE_VALUE | DBE_ALARM);
    dbScanUnlock(prec);

    testOk(subVal->npend == 2,
        "NULL field broadcast: VALUE sub got event (npend %lu)", subVal->npend);
    testOk(subAlarm->npend == 2,
        "NULL field broadcast: ALARM sub got event (npend %lu)", subAlarm->npend);

    /* Cleanup skipped */
}

/*
 * Test 6: db_add_event() input validation
 */
static void testAddEventValidation(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;

    testDiag("---- db_add_event validation ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chan = dbChannelCreate("x.VAL");
    if (!chan)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    sub = db_add_event(ctx, chan, noop_cb, NULL, 0);
    testOk(sub == NULL, "select==0 returns NULL");

    sub = db_add_event(ctx, chan, noop_cb, NULL, UCHAR_MAX + 1);
    testOk(sub == NULL, "select>UCHAR_MAX returns NULL");

    sub = db_add_event(ctx, chan, noop_cb, NULL, DBE_VALUE);
    testOk(sub != NULL, "valid select returns non-NULL");
    if (sub) {
        db_event_enable(sub);
        db_event_disable(sub);
        db_cancel_event(sub);
    }

    /* Cleanup skipped */
}

/*
 * Test 7: Flow control
 */
static void testFlowControl(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;
    struct cbContext cb;
    unsigned count;

    testDiag("---- flow control ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");
    if (db_start_events(ctx, "evtTest7", NULL, NULL,
            epicsThreadPriorityCAServerLow) != DB_EVENT_OK)
        testAbort("db_start_events failed");

    chan = dbChannelCreate("x.VAL");
    if (!chan)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    initCbContext(&cb);
    sub = db_add_event(ctx, chan, counting_cb, &cb, DBE_VALUE);
    if (!sub)
        testAbort("db_add_event failed");
    db_event_enable(sub);

    /* Baseline: post and drain */
    db_post_single_event(sub);
    syncEventTask(ctx);

    epicsMutexMustLock(cb.lock);
    count = cb.count;
    epicsMutexUnlock(cb.lock);
    testOk(count == 1, "baseline: callback invoked (got %u)", count);

    /* Enable flow control — delivery should stop */
    db_event_flow_ctrl_mode_on(ctx);

    db_post_single_event(sub);
    db_post_single_event(sub);
    syncEventTask(ctx);

    epicsMutexMustLock(cb.lock);
    count = cb.count;
    epicsMutexUnlock(cb.lock);
    testOk(count == 1, "flow ctrl on: count unchanged (got %u)", count);
    testOk(sub->npend > 0, "flow ctrl on: events queued (npend %lu)", sub->npend);

    /* Disable flow control — queued events should drain */
    db_event_flow_ctrl_mode_off(ctx);
    syncEventTask(ctx);

    epicsMutexMustLock(cb.lock);
    count = cb.count;
    epicsMutexUnlock(cb.lock);
    testOk(count > 1, "flow ctrl off: callbacks delivered (got %u)", count);
    testOk(sub->npend == 0, "flow ctrl off: queue drained (npend %lu)", sub->npend);

    db_event_disable(sub);
    db_cancel_event(sub);
    dbChannelDelete(chan);
    cleanCbContext(&cb);
    db_close_events(ctx);
}

/*
 * Test 8: Extra labor mechanism
 */
static void testExtraLabor(void)
{
    dbEventCtx ctx;

    testDiag("---- extra labor ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");
    if (db_start_events(ctx, "evtTest8", NULL, NULL,
            epicsThreadPriorityCAServerLow) != DB_EVENT_OK)
        testAbort("db_start_events failed");

    laborCount = 0;
    db_add_extra_labor_event(ctx, laborFunc, (void *)&laborCount);

    db_post_extra_labor(ctx);
    db_flush_extra_labor_event(ctx);
    testOk(laborCount == 1, "extra labor executed once (got %d)", laborCount);

    db_post_extra_labor(ctx);
    db_flush_extra_labor_event(ctx);
    testOk(laborCount == 2, "extra labor executed again (got %d)", laborCount);

    db_add_extra_labor_event(ctx, NULL, NULL);
    db_close_events(ctx);
}

/*
 * Test 9: db_cancel_event() immediate cleanup (no pending events)
 *
 * When npend==0 and no callback is in progress, db_cancel_event()
 * should free the subscription immediately and release quota.
 * Verify by creating a new subscription on the same queue afterwards.
 */
static void testCancelImmediate(void)
{
    dbEventCtx ctx;
    dbChannel *chan1, *chan2;
    dbEventSubscription sub1, sub2;

    testDiag("---- cancel immediate cleanup ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chan1 = dbChannelCreate("x.VAL");
    if (!chan1)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan1))
        testAbort("dbChannelOpen failed");

    sub1 = db_add_event(ctx, chan1, noop_cb, NULL, DBE_VALUE);
    if (!sub1)
        testAbort("db_add_event failed");
    db_event_enable(sub1);

    /* Cancel without posting any events (npend==0) */
    db_event_disable(sub1);
    db_cancel_event(sub1);

    /* sub1 is now freed — create another to verify quota was released */
    chan2 = dbChannelCreate("x.VAL");
    if (!chan2)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan2))
        testAbort("dbChannelOpen failed");

    sub2 = db_add_event(ctx, chan2, noop_cb, NULL, DBE_VALUE);
    testOk(sub2 != NULL, "new subscription after cancel succeeds");

    if (sub2) {
        db_event_enable(sub2);
        db_event_disable(sub2);
        db_cancel_event(sub2);
    }
    dbChannelDelete(chan1);
    dbChannelDelete(chan2);
    /* ctx leaked */
}

/*
 * Test 10: db_cancel_event() with pending events (deferred cleanup)
 *
 * When npend>0 but no callback is in progress, db_cancel_event()
 * sets user_sub=NULL and defers cleanup to the event task.
 */
static void testCancelWithPending(void)
{
    dbEventCtx ctx;
    dbChannel *chan;
    dbEventSubscription sub;

    testDiag("---- cancel with pending events ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    chan = dbChannelCreate("x.VAL");
    if (!chan)
        testAbort("dbChannelCreate failed");
    if (dbChannelOpen(chan))
        testAbort("dbChannelOpen failed");

    sub = db_add_event(ctx, chan, noop_cb, NULL, DBE_VALUE);
    if (!sub)
        testAbort("db_add_event failed");
    db_event_enable(sub);

    /* Post events to make npend > 0 */
    db_post_single_event(sub);
    db_post_single_event(sub);
    testOk(sub->npend > 0, "events pending before cancel (npend %lu)", sub->npend);

    /* Cancel — should defer cleanup since npend > 0 */
    db_event_disable(sub);
    db_cancel_event(sub);

    testOk(sub->user_sub == NULL, "user_sub set to NULL after cancel");

    /* ctx and sub leaked — no event task to complete deferred cleanup */
}

/*
 * Test 11: Overflow queue creation
 *
 * Fill the first queue's quota by creating EVENTSPERQUE subscriptions
 * (each takes EVENTENTRIES slots, total = EVENTQUESIZE).  A further
 * subscription should trigger overflow queue creation.
 */
static void testOverflowQueue(void)
{
#define OVERFLOW_SUBS (EVENTSPERQUE + 1)
    dbEventCtx ctx;
    dbChannel *chan[OVERFLOW_SUBS];
    dbEventSubscription sub[OVERFLOW_SUBS];
    int i;
    int allCreated = 1;

    testDiag("---- overflow queue ----");

    ctx = db_init_events();
    if (!ctx)
        testAbort("db_init_events failed");

    for (i = 0; i < OVERFLOW_SUBS; i++) {
        chan[i] = dbChannelCreate("x.VAL");
        if (!chan[i])
            testAbort("dbChannelCreate failed for sub %d", i);
        if (dbChannelOpen(chan[i]))
            testAbort("dbChannelOpen failed for sub %d", i);

        sub[i] = db_add_event(ctx, chan[i], noop_cb, NULL, DBE_VALUE);
        if (!sub[i]) {
            allCreated = 0;
            break;
        }
        db_event_enable(sub[i]);
    }

    testOk(allCreated, "all %d subscriptions created (overflow queue used)",
        OVERFLOW_SUBS);

    /* Post to the overflow subscription and verify it works */
    if (allCreated) {
        db_post_single_event(sub[OVERFLOW_SUBS - 1]);
        testOk(sub[OVERFLOW_SUBS - 1]->npend == 1,
            "overflow queue subscription got event (npend %lu)",
            sub[OVERFLOW_SUBS - 1]->npend);
    } else {
        testSkip(1, "overflow subscription not created");
    }

    /* Cleanup skipped */
#undef OVERFLOW_SUBS
}

MAIN(dbEventTest)
{
    testPlan(37);

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    iocBuildIsolated();
    eltc(1);

    testSingleSubscription();   /*  4 tests */
    testMultiSubscription();    /*  3 tests */
    testRefEventMetadata();     /*  5 tests */
    testCallbackDelivery();     /*  4 tests */
    testDbPostEventsFiltering();/*  6 tests */
    testAddEventValidation();   /*  3 tests */
    testFlowControl();          /*  5 tests */
    testExtraLabor();           /*  2 tests */
    testCancelImmediate();      /*  1 test  */
    testCancelWithPending();    /*  2 tests */
    testOverflowQueue();        /*  2 tests */

    return testDone();
}
