/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Periodic scanning with helper threads must keep the PHAS order a
 * sequential pass gives: every record of a lower PHAS has finished
 * before any record of a higher PHAS starts, and no record of the next
 * pass starts before the current pass is complete.
 */

#include <stdio.h>
#include <string.h>

#include "dbAccess.h"
#include "dbScan.h"
#include "epicsAtomic.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "errlog.h"
#include "iocInit.h"

#include "dbUnitTest.h"
#include "testMain.h"
#include "menuScan.h"
#include "xRecord.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

#define NPHASE 3
#define MAXTHREADS 128

/* records of the fast list ("0.1 second") per PHAS */
static const int nrec[NPHASE] = {24, 4, 2};
/* records of the slow list ("1 second"), PHAS 0 */
#define NSLOW 6

static int started[NPHASE];     /* atomic */
static int done[NPHASE];        /* atomic */
static int violations;          /* atomic */
static int slowStarted;         /* atomic */
static int slowHelpersNow;      /* atomic: helpers inside a slow record */
static int slowHelpersMax;      /* atomic */

static epicsMutexId tidLock;
static epicsThreadId tids[MAXTHREADS];
static int ntids;

static void noteThread(void)
{
    epicsThreadId me = epicsThreadGetIdSelf();
    int i;

    epicsMutexMustLock(tidLock);
    for (i = 0; i < ntids; i++)
        if (tids[i] == me) break;
    if (i == ntids && ntids < MAXTHREADS)
        tids[ntids++] = me;
    epicsMutexUnlock(tidLock);
}

static void fastProc(xRecord *prec)
{
    int k = prec->phas;
    int s = epicsAtomicIncrIntT(&started[k]);
    int pass = (s - 1) / nrec[k];

    /* all of the lower PHAS of this pass done, none of the next pass */
    if (k > 0 && epicsAtomicGetIntT(&done[k - 1]) != nrec[k - 1] * (pass + 1)) {
        epicsAtomicIncrIntT(&violations);
        testDiag("%s: PHAS %d started with done[%d]=%d, expected %d",
            prec->name, k, k - 1, epicsAtomicGetIntT(&done[k - 1]),
            nrec[k - 1] * (pass + 1));
    }
    /* the higher PHAS of this pass has not started */
    if (k + 1 < NPHASE &&
        epicsAtomicGetIntT(&started[k + 1]) != nrec[k + 1] * pass) {
        epicsAtomicIncrIntT(&violations);
        testDiag("%s: PHAS %d started with started[%d]=%d, expected %d",
            prec->name, k, k + 1, epicsAtomicGetIntT(&started[k + 1]),
            nrec[k + 1] * pass);
    }
    if (k == 0)
        noteThread();
    epicsThreadSleep(0.001);
    epicsAtomicIncrIntT(&done[k]);
}

static void slowProc(xRecord *prec)
{
    int helper = strncmp(epicsThreadGetNameSelf(), "scanHelper", 10) == 0;

    epicsAtomicIncrIntT(&slowStarted);
    if (helper) {
        int now = epicsAtomicIncrIntT(&slowHelpersNow);
        int max = epicsAtomicGetIntT(&slowHelpersMax);

        while (now > max) {
            int seen = epicsAtomicCmpAndSwapIntT(&slowHelpersMax, max, now);

            if (seen == max) break;
            max = seen;
        }
    }
    epicsThreadSleep(0.005);
    if (helper)
        epicsAtomicDecrIntT(&slowHelpersNow);
}

static void loadRecords(void)
{
    char subs[64];
    int k, i;

    for (k = 0; k < NPHASE; k++) {
        for (i = 0; i < nrec[k]; i++) {
            sprintf(subs, "NAME=fast%d_%d,SCAN=.1 second,PHAS=%d", k, i, k);
            testdbReadDatabase("dbScanParallelTest.db", NULL, subs);
        }
    }
    for (i = 0; i < NSLOW; i++) {
        sprintf(subs, "NAME=slow%d,SCAN=1 second,PHAS=0", i);
        testdbReadDatabase("dbScanParallelTest.db", NULL, subs);
    }
}

static void hookRecords(void)
{
    char name[32];
    int k, i;

    for (k = 0; k < NPHASE; k++) {
        for (i = 0; i < nrec[k]; i++) {
            sprintf(name, "fast%d_%d", k, i);
            ((xRecord *)testdbRecordPtr(name))->clbk = fastProc;
        }
    }
    for (i = 0; i < NSLOW; i++) {
        sprintf(name, "slow%d", i);
        ((xRecord *)testdbRecordPtr(name))->clbk = slowProc;
    }
}

/* After iocPause() no new pass starts, but one may be in flight: a pass
 * is complete when every PHAS has started and finished the same number
 * of passes as PHAS 0. On a slow target a pass can take seconds. */
static void waitPassComplete(void)
{
    int i, k;

    for (i = 0; i < 600; i++) {
        int passes = epicsAtomicGetIntT(&started[0]) / nrec[0];

        for (k = 0; k < NPHASE; k++) {
            if (epicsAtomicGetIntT(&started[k]) != nrec[k] * passes ||
                epicsAtomicGetIntT(&done[k]) != nrec[k] * passes)
                break;
        }
        if (k == NPHASE)
            return;
        epicsThreadSleep(0.05);
    }
    testDiag("pass still in flight after 30 s");
}

/* the helper threads that exist, by name */
static int countHelperThreads(void)
{
    char name[32];
    int n = 0;

    for (;;) {
        sprintf(name, "scanHelper%d", n);
        if (!epicsThreadGetId(name))
            return n;
        n++;
    }
}

/* Idle helpers sleep at the priority a wake-up needs: dedicated ones at
 * their rate's, pool helpers at the fastest rate's. The dedicated
 * helpers come first, slow rate before fast, then the pool. */
static int helpersAtWrongPriority(int nhelpers, int fastT, int slowT)
{
    unsigned base = epicsThreadPriorityScanLow - SCAN_1ST_PERIODIC;
    char name[32];
    int i, wrong = 0;

    for (i = 0; i < nhelpers; i++) {
        unsigned expect = i < slowT ? base + menuScan1_second :
            i < slowT + fastT ? base + menuScan_1_second :
            base + menuScan_NUM_CHOICES - 1;
        unsigned got;

        sprintf(name, "scanHelper%d", i);
        got = epicsThreadGetPriority(epicsThreadGetId(name));
        if (got != expect) {
            testDiag("%s sleeps at priority %u, expected %u", name, got,
                expect);
            wrong++;
        }
    }
    return wrong;
}

/* helpers < 0: leave the scan system unconfigured (no helpers), 0: the
 * default count; fastT / slowT: dedicated helpers for the fast and the
 * slow rate */
static void runWith(int helpers, int reserved, int fastT, int slowT,
    double seconds)
{
    int k, passes, nslow, nthreads, cap, pool, nhelpers;

    if (helpers < 0)
        testDiag("no scanParallelThreads() for %.1f s", seconds);
    else
        testDiag("scanParallelThreads(%d, %d) for %.1f s", helpers, reserved,
            seconds);
    if (fastT || slowT)
        testDiag("scanRateThreads: %d for the fast, %d for the slow rate",
            fastT, slowT);
    memset(started, 0, sizeof started);
    memset(done, 0, sizeof done);
    violations = 0;
    slowStarted = 0;
    slowHelpersNow = 0;
    slowHelpersMax = 0;
    ntids = 0;

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    loadRecords();
    hookRecords();

    if (helpers >= 0)
        testOk1(scanParallelThreads(helpers, reserved) == 0);
    else
        testSkip(1, "unconfigured");
    /* always set both, as the setting outlives iocShutdown() */
    testOk1(scanRateThreads(".1 second", fastT) == 0);
    testOk1(scanRateThreads("1 second", slowT) == 0);
    testOk1(scanRateThreads("sometimes", 1) != 0);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testOk(scanParallelThreads(helpers, reserved) != 0,
        "scanParallelThreads refused after iocInit");
    /* the default is scanParallelThreadsDefault, but never more than
     * CPUs - 1: a single CPU gets no pool at all */
    pool = helpers < 0 ? 0 : helpers > 0 ? helpers : scanParallelThreadsDefault;
    if (helpers == 0 && pool > epicsThreadGetCPUs() - 1)
        pool = epicsThreadGetCPUs() - 1;
    nhelpers = countHelperThreads();
    testOk(nhelpers == pool + fastT + slowT, "%d helper threads, expected %d",
        nhelpers, pool + fastT + slowT);
    testOk(scanRateThreads("1 second", 1) != 0,
        "scanRateThreads refused after iocInit");

    epicsThreadSleep(seconds);

    /* iocShutdown marks records PACT while a pass may still be running,
     * so pause first and wait for the pass in progress to finish */
    testOk1(iocPause() == 0);
    waitPassComplete();
    /* the helpers have returned to sleep once their last slot is done */
    epicsThreadSleep(0.1);
    testOk(helpersAtWrongPriority(nhelpers, fastT, slowT) == 0,
        "idle helpers sleep at their wake-up priority");

    /* the scan threads are idle now; atomic reads for the sanitizer's sake */
    passes = epicsAtomicGetIntT(&started[0]) / nrec[0];
    nslow = epicsAtomicGetIntT(&slowStarted);
    epicsMutexMustLock(tidLock);
    nthreads = ntids;
    epicsMutexUnlock(tidLock);
    testDiag("%d passes of the fast list, %d slow records, %d threads",
        passes, nslow, nthreads);
    /* each record sleeps 1 ms, which on Windows and loaded CI runners
     * takes a sleep quantum of 10-15 ms, so passes overrun their period:
     * only ask for enough passes to cross a pass boundary */
    testOk(passes >= 2, "enough passes: %d", passes);
    testOk(epicsAtomicGetIntT(&violations) == 0, "PHAS order violations: %d",
        epicsAtomicGetIntT(&violations));
    for (k = 0; k < NPHASE; k++) {
        int s = epicsAtomicGetIntT(&started[k]);
        int d = epicsAtomicGetIntT(&done[k]);

        testOk(s == nrec[k] * passes,
            "PHAS %d started %d times for %d passes", k, s, passes);
        testOk(d == s, "PHAS %d done %d of %d started", k, d, s);
    }
    testOk(nslow >= NSLOW, "slow list ran: %d", nslow);
    /* pool helpers on the slow list: reserve 0 means the default of one
     * kept free, negative none; dedicated slow helpers on top */
    cap = reserved == 0 ? 1 : reserved < 0 ? 0 : reserved;
    cap = pool > cap ? pool - cap : 0;
    cap += slowT;
    testOk(epicsAtomicGetIntT(&slowHelpersMax) <= cap,
        "at most %d helpers on the slow list at once: %d", cap,
        epicsAtomicGetIntT(&slowHelpersMax));
    if (pool || fastT)
        testOk(nthreads > 1, "helpers took part: %d threads", nthreads);
    else
        testOk(nthreads == 1, "leader alone: %d threads", nthreads);
    if (fastT && !pool)
        testOk(nthreads == fastT + 1, "exactly the dedicated helpers: %d",
            nthreads);
    else
        testSkip(1, "pool present");

    testIocShutdownOk();
    testdbCleanup();
}

MAIN(dbScanParallelTest)
{
    testPlan(6 * 21);
    tidLock = epicsMutexMustCreate();
    /* the pool setting also outlives iocShutdown(): unconfigured first */
    runWith(-1, 0, 0, 0, 2.0);
    runWith(-1, 0, 3, 2, 2.0);
    runWith(2, 0, 0, 0, 2.0);
    runWith(2, -1, 1, 1, 2.0);
    runWith(8, 6, 0, 0, 3.0);
    runWith(0, 0, 0, 0, 2.0);
    epicsMutexDestroy(tidLock);
    return testDone();
}
