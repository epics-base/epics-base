/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2013 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <stdlib.h>

#include "epicsEvent.h"
#include "epicsMessageQueue.h"
#include "epicsPrint.h"
#include "epicsMath.h"
#include "alarm.h"
#include "menuPriority.h"
#include "dbChannel.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "dbScan.h"
#include "dbLock.h"
#include "dbUnitTest.h"
#include "dbCommon.h"
#include "registry.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "recSup.h"
#include "devSup.h"
#include "iocInit.h"
#include "callback.h"
#include "ellLib.h"
#include "epicsUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

#define GEN_SIZE_OFFSET
#include "yRecord.h"

#include "epicsExport.h"

#define ONE_THREAD_LOOPS 101
#define PAR_THREAD_LOOPS 53
#define CB_THREAD_LOOPS 13

#define NO_OF_THREADS 7
#define NO_OF_MEMBERS 5
#define NO_OF_GROUPS 11

#define NO_OF_MID_THREADS 3

static int noOfGroups = NO_OF_GROUPS;
static int noOfIoscans = NO_OF_GROUPS;

static IOSCANPVT *ioscanpvt;      /* Soft interrupt sources */
static ELLLIST *pvtList;          /* Per group private part lists */

static int executionOrder;
static int orderFail;
static int testNo;
static epicsMessageQueueId *mq;   /* Per group message queue */
static epicsEventId *barrier;     /* Per group barrier event */
static int *cbCounter;

struct pvtY {
    ELLNODE node;
    yRecord *prec;
    int group;
    int member;
    int count;
    int processed;
    int callback;
};

/* test2: priority and ioscan index for each group
 *     used priorities are expressed in the bit pattern of (ioscan index + 1) */
struct groupItem {
    int prio;
    int ioscan;
} groupTable[12] = {
    { 0, 0 },
    { 1, 1 },
    { 0, 2 }, { 1, 2 },
    { 2, 3 },
    { 0, 4 }, { 2, 4 },
    { 1, 5 }, { 2, 5 },
    { 0, 6 }, { 1, 6 }, { 2, 6 }
};
static int recsProcessed = 1;
static int noDoubleCallback = 1;

void scanIoTest_registerRecordDeviceDriver(struct dbBase *);

long count_bits(long n) {
  unsigned int c; /* c accumulates the total bits set in v */
  for (c = 0; n; c++)
    n &= n - 1; /* clear the least significant bit set */
  return c;
}

/*************************************************************************\
* yRecord: minimal record needed to test I/O Intr scanning
\*************************************************************************/

static long get_ioint_info(int cmd, yRecord *prec, IOSCANPVT *ppvt)
{
    struct pvtY *pvt = (struct pvtY *)(prec->dpvt);

    if (testNo == 2)
        *ppvt = ioscanpvt[groupTable[pvt->group].ioscan];
    else
        *ppvt = ioscanpvt[pvt->group];
    return 0;
}

struct ydset {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN process;
} devY = {
    5,
    NULL,
    NULL,
    NULL,
    get_ioint_info,
    NULL
};
epicsExportAddress(dset, devY);

static long init_record(yRecord *prec, int pass)
{
    struct pvtY *pvt;

    if (pass == 0) return 0;

    pvt = (struct pvtY *) calloc(1, sizeof(struct pvtY));
    prec->dpvt = pvt;

    pvt->prec    = prec;
    sscanf(prec->name, "g%dm%d", &pvt->group, &pvt->member);
    ellAdd(&pvtList[pvt->group], &pvt->node);

    return 0;
}

static long process(yRecord *prec)
{
    struct pvtY *pvt = (struct pvtY *)(prec->dpvt);

    if (testNo == 0) {
        /* Single callback thread */
        if (executionOrder != pvt->member) {
            orderFail = 1;
        }
        pvt->count++;
        if (++executionOrder == NO_OF_MEMBERS) executionOrder = 0;
    } else {
        pvt->count++;
        if (pvt->member == 0) {
            epicsMessageQueueSend(mq[pvt->group], NULL, 0);
            epicsEventMustWait(barrier[pvt->group]);
        }
    }
    pvt->processed = 1;
    return 0;
}

rset yRSET={
    4,
    NULL, /* report */
    NULL, /* initialize */
    init_record,
    process
};
epicsExportAddress(rset, yRSET);

static void startMockIoc(void) {
    char substitutions[256];
    int i, j;
    char *prio[] = { "LOW", "MEDIUM", "HIGH" };

    if (testNo == 2) {
        noOfGroups = 12;
        noOfIoscans = 7;
    }
    ioscanpvt = calloc(noOfIoscans, sizeof(IOSCANPVT));
    mq = calloc(noOfGroups, sizeof(epicsMessageQueueId));
    barrier = calloc(noOfGroups, sizeof(epicsEventId));
    pvtList = calloc(noOfGroups, sizeof(ELLLIST));
    cbCounter = calloc(noOfGroups, sizeof(int));

    if (dbReadDatabase(&pdbbase, "scanIoTest.dbd",
            "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
            "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL))
        testAbort("Error reading database description 'scanIoTest.dbd'");

    callbackParallelThreads(1, "Low");
    callbackParallelThreads(NO_OF_MID_THREADS, "Medium");
    callbackParallelThreads(NO_OF_THREADS, "High");

    for (i = 0; i < noOfIoscans; i++) {
        scanIoInit(&ioscanpvt[i]);
    }

    for (i = 0; i < noOfGroups; i++) {
        mq[i] = epicsMessageQueueCreate(NO_OF_MEMBERS, 1);
        barrier[i] = epicsEventMustCreate(epicsEventEmpty);
        ellInit(&pvtList[i]);
    }

    scanIoTest_registerRecordDeviceDriver(pdbbase);
    for (i = 0; i < noOfGroups; i++) {
        for (j = 0; j < NO_OF_MEMBERS; j++) {
            sprintf(substitutions, "GROUP=%d,MEMBER=%d,PRIO=%s", i, j,
                    testNo==0?"LOW":(testNo==1?"HIGH":prio[groupTable[i].prio]));
            if (dbReadDatabase(&pdbbase, "scanIoTest.db",
                    "." OSI_PATH_LIST_SEPARATOR "..", substitutions))
                testAbort("Error reading test database 'scanIoTest.db'");
        }
    }

    testIocInitOk();
}

static void stopMockIoc(void) {
    int i;

    testIocShutdownOk();
    epicsThreadSleep(0.1);
    for (i = 0; i < noOfGroups; i++) {
        epicsMessageQueueDestroy(mq[i]); mq[i] = NULL;
        epicsEventDestroy(barrier[i]); barrier[i] = NULL;
        ellFree(&pvtList[i]);
    }
    free(mq);
    free(barrier);
    free(pvtList);
    free(cbCounter);
    testdbCleanup();
}

static void checkProcessed(void *user, IOSCANPVT ioscan, int prio) {
    struct pvtY *pvt;
    int group = -1;
    int i;

    for (i = 0; i < noOfGroups; i++) {
        if (ioscanpvt[groupTable[i].ioscan] == ioscan
                && groupTable[i].prio == prio) {
            group = i;
            break;
        }
    }
    if (group == -1)
        testAbort("invalid ioscanpvt in scanio callback");

    cbCounter[group]++;
    for (pvt = (struct pvtY *)ellFirst(&pvtList[group]);
         pvt;
         pvt = (struct pvtY *)ellNext(&pvt->node)) {
        if (pvt->callback == 1) {
            testDiag("callback for rec %s arrived twice\n", pvt->prec->name);
            noDoubleCallback = 0;
        }
        if (pvt->processed == 0) {
            testDiag("rec %s was not processed\n", pvt->prec->name);
            recsProcessed = 0;
        }
        pvt->callback = 1;
    }
}

/*************************************************************************\
* scanIoTest: Test I/O Intr scanning
* including parallel callback threads and scanio callbacks
\*************************************************************************/

MAIN(scanIoTest)
{
    int i, j;
    int loop;
    int max_one, max_one_all;
    int parallel, parallel_all;
    int result;
    int cbCountOk;
    long waiting;
    struct pvtY *pvt;

    testPlan(10);

    if (noOfGroups < NO_OF_THREADS)
        testAbort("ERROR: This test requires number of ioscan sources >= number of parallel threads");

    /**************************************\
    * Single callback thread
    \**************************************/

    testNo = 0;
    startMockIoc();

    testDiag("Testing single callback thread");
    testDiag("    using %d ioscan sources, %d records for each, and %d loops",
             noOfGroups, NO_OF_MEMBERS, ONE_THREAD_LOOPS);

    for (j = 0; j < ONE_THREAD_LOOPS; j++) {
        for (i = 0; i < noOfIoscans; i++) {
            scanIoRequest(ioscanpvt[i]);
        }
    }

    epicsThreadSleep(1.0);

    testOk((orderFail==0), "No out-of-order processing");

    result = 1;
    for (i = 0; i < noOfGroups; i++) {
        for (pvt = (struct pvtY *)ellFirst(&pvtList[i]);
             pvt;
             pvt = (struct pvtY *)ellNext(&pvt->node)) {
            if (pvt->count != ONE_THREAD_LOOPS) result = 0;
        }
    }

    testOk(result, "All per-record process counters match number of loops");

    stopMockIoc();

    /**************************************\
    * Multiple parallel callback threads
    \**************************************/

    testNo = 1;
    startMockIoc();

    testDiag("Testing multiple parallel callback threads");
    testDiag("    using %d ioscan sources, %d records for each, %d loops, and %d parallel threads",
             noOfIoscans, NO_OF_MEMBERS, PAR_THREAD_LOOPS, NO_OF_THREADS);

    for (j = 0; j < PAR_THREAD_LOOPS; j++) {
        for (i = 0; i < noOfIoscans; i++) {
            scanIoRequest(ioscanpvt[i]);
        }
    }

    /* With parallel cb threads, order and distribution to threads are not guaranteed.
     * We have stop barrier events for each request (in the first record).
     * Test schedule:
     * - After the requests have been put in the queue, NO_OF_THREADS threads should have taken
     *   one request each.
     * - Each barrier event is given PAR_THREAD_LOOPS times.
     * - Whenever things stop, there should be four threads waiting, one request each.
     * - After all loops, each record should have processed PAR_THREAD_LOOPS times.
     */

    max_one_all = 1;
    parallel_all = 1;

    for (loop = 0; loop < (PAR_THREAD_LOOPS * noOfGroups) / NO_OF_THREADS + 1; loop++) {
        max_one = 1;
        parallel = 0;
        waiting = 0;
        j = 0;
        do {
            epicsThreadSleep(0.001);
            j++;
            for (i = 0; i < noOfGroups; i++) {
                int l = epicsMessageQueuePending(mq[i]);
                while (epicsMessageQueueTryReceive(mq[i], NULL, 0) != -1);
                if (l == 1) {
                    waiting |= 1 << i;
                } else if (l > 1) {
                    max_one = 0;
                }
            }
            parallel = count_bits(waiting);
        } while (j < 5 && parallel < NO_OF_THREADS);

        if (!max_one) max_one_all = 0;
        if (loop < (PAR_THREAD_LOOPS * noOfGroups) / NO_OF_THREADS) {
            if (!(parallel == NO_OF_THREADS)) parallel_all = 0;
        } else {
            /* In the last run of the loop only the remaining requests are processed */
            if (!(parallel == PAR_THREAD_LOOPS * noOfGroups % NO_OF_THREADS)) parallel_all = 0;
        }

        for (i = 0; i < noOfGroups; i++) {
            if (waiting & (1 << i)) {
                epicsEventTrigger(barrier[i]);
            }
        }
    }

    testOk(max_one_all, "No thread took more than one request per loop");
    testOk(parallel_all, "Correct number of requests were being processed in parallel in each loop");

    epicsThreadSleep(0.1);

    result = 1;
    for (i = 0; i < noOfGroups; i++) {
        for (pvt = (struct pvtY *)ellFirst(&pvtList[i]);
             pvt;
             pvt = (struct pvtY *)ellNext(&pvt->node)) {
            if (pvt->count != PAR_THREAD_LOOPS) {
                testDiag("Process counter for record %s (%d) does not match loop count (%d)",
                         pvt->prec->name, pvt->count, PAR_THREAD_LOOPS);
                result = 0;
            }
        }
    }

    testOk(result, "All per-record process counters match number of loops");

    stopMockIoc();

    /**************************************\
    * Scanio callback mechanism
    \**************************************/

    testNo = 2;
    startMockIoc();

    for (i = 0; i < noOfIoscans; i++) {
        scanIoSetComplete(ioscanpvt[i], checkProcessed, NULL);
    }

    testDiag("Testing scanio callback mechanism");
    testDiag("    using %d ioscan sources, %d records for each, %d loops, and 1 LOW / %d MEDIUM / %d HIGH parallel threads",
             noOfIoscans, NO_OF_MEMBERS, CB_THREAD_LOOPS, NO_OF_MID_THREADS, NO_OF_THREADS);

    result = 1;
    for (j = 0; j < CB_THREAD_LOOPS; j++) {
        for (i = 0; i < noOfIoscans; i++) {
            int prio_used;
            prio_used = scanIoRequest(ioscanpvt[i]);
            if (i+1 != prio_used)
                result = 0;
        }
    }
    testOk(result, "All requests return the correct priority callback mask (all 7 permutations covered)");

    /* Test schedule:
     * After the requests have been put in the queue, it is checked
     * - that each callback arrives exactly once,
     * - after all records in the group have been processed.
     */

    /* loop count times 4 since (worst case) one loop triggers 4 groups for the single LOW thread */
    for (loop = 0; loop < CB_THREAD_LOOPS * 4; loop++) {
        max_one = 1;
        parallel = 0;
        waiting = 0;
        j = 0;
        do {
            epicsThreadSleep(0.001);
            j++;
            for (i = 0; i < noOfGroups; i++) {
                int l = epicsMessageQueuePending(mq[i]);
                while (epicsMessageQueueTryReceive(mq[i], NULL, 0) != -1);
                if (l == 1) {
                    waiting |= 1 << i;
                } else if (l > 1) {
                    max_one = 0;
                }
            }
            parallel = count_bits(waiting);
        } while (j < 5);
\
        for (i = 0; i < noOfGroups; i++) {
            if (waiting & (1 << i)) {
                for (pvt = (struct pvtY *)ellFirst(&pvtList[i]);
                     pvt;
                     pvt = (struct pvtY *)ellNext(&pvt->node)) {
                    pvt->processed = 0;
                    pvt->callback = 0;
                    /* record processing will set this at the end of process() */
                }
                epicsEventTrigger(barrier[i]);
            }
        }
    }

    epicsThreadSleep(0.1);

    testOk(recsProcessed, "Each callback occured after all records in the group were processed");
    testOk(noDoubleCallback, "No double callbacks occured in any loop");

    result = 1;
    cbCountOk = 1;
    for (i = 0; i < noOfGroups; i++) {
        if (cbCounter[i] != CB_THREAD_LOOPS) {
            testDiag("Callback counter for group %d (%d) does not match loop count (%d)",
                     i, cbCounter[i], CB_THREAD_LOOPS);
            cbCountOk = 0;
        }
        for (pvt = (struct pvtY *)ellFirst(&pvtList[i]);
             pvt;
             pvt = (struct pvtY *)ellNext(&pvt->node)) {
            if (pvt->count != CB_THREAD_LOOPS) {
                testDiag("Process counter for record %s (%d) does not match loop count (%d)",
                         pvt->prec->name, pvt->count, CB_THREAD_LOOPS);
                result = 0;
            }
        }
    }

    testOk(result, "All per-record process counters match number of loops");
    testOk(cbCountOk, "All per-group callback counters match number of loops");

    stopMockIoc();

    return testDone();
}
