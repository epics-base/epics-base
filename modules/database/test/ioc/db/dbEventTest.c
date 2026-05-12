/*************************************************************************\
* Copyright (c) 2026 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Test that the dbEvent queue replacement threshold scales with the
 * actual number of subscriptions rather than a hard-coded constant.
 *
 * Strategy: use iocBuildIsolated() with db_init_events() but without
 * db_start_events(), so the event task never drains the queue.
 * Post events via db_post_single_event() and observe npend/nreplace
 * on the subscription (accessible via EPICS_PRIVATE_API).
 *
 * For useValque=TRUE subscriptions (simple scalar fields), every posted
 * event creates a dbfl_type_val field log which passes the duplicate
 * check at db_queue_event_log line 794 and reaches the replacement
 * threshold check.
 */

#define EPICS_PRIVATE_API
#define USE_TYPED_DBEVENT

#include <string.h>

#include "alarm.h"
#include "dbAccess.h"
#include "dbChannel.h"
#include "dbEvent.h"
#include "dbLock.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "iocInit.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#include "osiFileName.h"

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

/*
 * Test 1: Single subscription
 *
 * With one subscription, quota = EVENTENTRIES and the replacement
 * threshold is quota/EVENTENTRIES = 1.  The ring has EVENTQUESIZE
 * slots total.  We should be able to queue EVENTQUESIZE-1 events
 * before replacement kicks in (rngSpace must drop to <= 1).
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

    /* Verify initial state */
    testOk(sub->npend == 0, "npend initially 0 (got %lu)", sub->npend);
    testOk(sub->nreplace == 0, "nreplace initially 0 (got %lu)", sub->nreplace);

    /* Post EVENTQUESIZE events */
    for (i = 0; i < EVENTQUESIZE; i++)
        db_post_single_event(sub);

    /*
     * With the corrected threshold (rngSpace <= 1 for one subscription):
     *   - Events 1..143 queue normally (npend goes 1..143)
     *   - Event 144: rngSpace=1, triggers replacement (nreplace=1)
     *
     * With the old threshold (rngSpace <= 36):
     *   - Events 1..108 queue normally
     *   - Events 109..144: 36 replacements
     */
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
 *
 * With N subscriptions on the same field, quota = N*EVENTENTRIES and
 * the replacement threshold is N.  Each db_post_single_event() on one
 * subscription adds one entry to the shared ring, so the ring fills
 * N times faster per subscription.
 *
 * Use 4 subscriptions.  Threshold = 4.
 * Post events to all 4 in round-robin order.
 * After filling EVENTQUESIZE-4 slots (140), the next round of 4
 * should trigger replacement on all of them since rngSpace <= 4.
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

    /* Post events in round-robin until the ring is full.
     * EVENTQUESIZE / NSUBS = 36 rounds fills all slots.
     */
    for (i = 0; i < EVENTQUESIZE; i++)
        db_post_single_event(sub[i % NSUBS]);

    total_npend = 0;
    total_nreplace = 0;
    for (i = 0; i < NSUBS; i++) {
        total_npend += sub[i]->npend;
        total_nreplace += sub[i]->nreplace;
    }

    /*
     * With the corrected threshold (rngSpace <= 4):
     *   - First 140 events (35 rounds) queue normally
     *   - Last 4 events (round 36): rngSpace goes 4,3,2,1 — all trigger replacement
     *   - total_nreplace = 4, total_npend = 140
     *
     * With the old threshold (rngSpace <= 36):
     *   - Far more replacements would occur
     */
    testOk(total_npend + total_nreplace == EVENTQUESIZE,
        "multi sub: npend+nreplace == %d (got %lu)",
        EVENTQUESIZE, total_npend + total_nreplace);
    testOk(total_nreplace == NSUBS,
        "multi sub: total nreplace == %d (got %lu)", NSUBS, total_nreplace);
    testOk(total_npend == EVENTQUESIZE - NSUBS,
        "multi sub: total npend == %d (got %lu)",
        EVENTQUESIZE - NSUBS, total_npend);

    /* Cleanup skipped, see comment in testSingleSubscription() */
#undef NSUBS
}

/*
 * Test 3: Duplicate reference events must preserve new metadata
 *
 * When both the queued event and a new event are reference-type field
 * logs (both point to the same record field), db_queue_event_log()
 * detects a duplicate and keeps only one.  The survivor must carry
 * the most recent alarm status/severity/timestamp, since those are
 * copied into the field log at creation time from the record and
 * may have changed between the two posts.
 *
 * Use the DESC field (DBF_STRING, fieldSize=41 > sizeof(union native_value))
 * so that useValque=FALSE and field logs are dbfl_type_ref.
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

    /* Post first event with no alarm */
    dbScanLock(prec);
    prec->stat = 0;
    prec->sevr = 0;
    dbScanUnlock(prec);
    db_post_single_event(sub);

    testOk(sub->npend == 1, "one event pending after first post (got %lu)",
        sub->npend);

    /* Change alarm state on the record */
    dbScanLock(prec);
    prec->stat = HIHI_ALARM;
    prec->sevr = MAJOR_ALARM;
    dbScanUnlock(prec);

    /* Post second event — triggers the duplicate reference check */
    db_post_single_event(sub);

    testOk(sub->npend == 1,
        "still one event pending after duplicate (got %lu)", sub->npend);

    /*
     * The queued field log must carry the NEWER alarm metadata.
     *
     * Bug: the old code at line 797 deletes the new field log and
     * keeps the old one, losing the updated alarm status/severity.
     */
    pfl = *sub->pLastLog;
    testOk(pfl->sevr == MAJOR_ALARM,
        "queued event has new severity %d (expected %d)",
        pfl->sevr, MAJOR_ALARM);
    testOk(pfl->stat == HIHI_ALARM,
        "queued event has new alarm status %d (expected %d)",
        pfl->stat, HIHI_ALARM);

    /* Cleanup skipped, see comment in testSingleSubscription() */
}

MAIN(dbEventTest)
{
    testPlan(12);

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    iocBuildIsolated();
    eltc(1);

    testSingleSubscription();
    testMultiSubscription();
    testRefEventMetadata();

    return testDone();
}
