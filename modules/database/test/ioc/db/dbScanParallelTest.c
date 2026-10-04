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
static int reserve;             /* helpers reserved for the fast list */
static int reservedOnSlow;      /* atomic */

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
    int idx;

    epicsAtomicIncrIntT(&slowStarted);
    if (sscanf(epicsThreadGetNameSelf(), "scanHelper%d", &idx) == 1 &&
        idx < reserve) {
        epicsAtomicIncrIntT(&reservedOnSlow);
        testDiag("%s processed by reserved helper %d", prec->name, idx);
    }
    epicsThreadSleep(0.005);
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

/* helpers < 0: leave the scan system unconfigured (no helpers) */
static void runWith(int helpers, int reserved, double seconds)
{
    int k, passes, nslow, nthreads;

    if (helpers < 0)
        testDiag("no scanParallelThreads() for %.1f s", seconds);
    else
        testDiag("scanParallelThreads(%d, %d) for %.1f s", helpers, reserved,
            seconds);
    memset(started, 0, sizeof started);
    memset(done, 0, sizeof done);
    violations = 0;
    slowStarted = 0;
    reserve = reserved;
    reservedOnSlow = 0;
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

    eltc(0);
    testIocInitOk();
    eltc(1);

    testOk(scanParallelThreads(helpers, reserved) != 0,
        "scanParallelThreads refused after iocInit");

    epicsThreadSleep(seconds);

    /* iocShutdown marks records PACT while a pass may still be running,
     * so pause first and let the pass in progress finish */
    testOk1(iocPause() == 0);
    epicsThreadSleep(0.5);

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
    testOk(epicsAtomicGetIntT(&reservedOnSlow) == 0,
        "reserved helpers on the slow list: %d",
        epicsAtomicGetIntT(&reservedOnSlow));
    if (helpers >= 0)
        testOk(nthreads > 1, "helpers took part: %d threads", nthreads);
    else
        testOk(nthreads == 1, "leader alone: %d threads", nthreads);

    testIocShutdownOk();
    testdbCleanup();
}

MAIN(dbScanParallelTest)
{
    testPlan(3 * 14);
    tidLock = epicsMutexMustCreate();
    runWith(-1, 0, 2.0);
    runWith(2, 0, 2.0);
    runWith(8, 3, 3.0);
    epicsMutexDestroy(tidLock);
    return testDone();
}
