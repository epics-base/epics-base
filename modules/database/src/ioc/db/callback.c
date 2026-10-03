/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* callback.c */

/* general purpose callback tasks               */
/*
 *      Original Author:        Marty Kraimer
 *      Date:                   07-18-91
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cantProceed.h"
#include "dbDefs.h"
#include <stdint.h>

#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsInterrupt.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsTimer.h"
#include "errlog.h"
#include "errMdef.h"
#include "taskwd.h"

#include "callback.h"
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "dbStaticLib.h"
#include "epicsExport.h"
#include "link.h"
#include "recSup.h"
#include "dbUnitTest.h" /* for testSyncCallback() */


static int callbackQueueSize = 2000;

/* Requests take a node from a lock-free pool of callbackQueueSize
 * nodes (the pool running out is the "buffer full" error) and push it
 * onto a LIFO inbox with one CAS; nothing on the request path has an
 * owner, so a preempted requester never blocks a worker. Workers pop
 * one node at a time from a shared ready stack; the worker that finds
 * it empty takes the whole inbox with one CAS, reverses it to FIFO,
 * runs the first node and pushes the rest onto the stack, so a
 * callback that blocks holds back no other. The stack head is a pool
 * index with an ABA tag, like the free list's.
 *
 * Each worker has its own event and a state word (AWAKE, SLEEPING or
 * CLAIMED, tagged with the sleep epoch). A waker triggers the event
 * first and claims the worker by CAS second, so a thread stopped
 * between the two leaves nothing others wait for, and a claim never
 * lands on a later sleep. A sleeping worker's bit in sleepers is a
 * hint, taken by CAS by whoever wants that sleeper; nAwake counts the
 * workers not sleeping, and a waker that finds no bit while nAwake
 * says one sleeps scans the state words instead.
 *
 * A requester wakes a worker when none is awake, or when one sleeps
 * while every awake one is busy: a worker marks itself busy before
 * each callback and only then looks at the queue and the sleepers, so
 * either the request sees the mark or the worker sees the request and
 * wakes a sleeper for it. Only a request that found the inbox empty
 * owes that check, as one already there has a worker on its way whose
 * take-all covers both. One rescue remains for a worker preempted
 * where it cannot migrate: when a single worker is counted awake and
 * none made progress for CB_STALE_US, the requester wakes another.
 */
#ifndef CB_FREE_EVERY
#define CB_FREE_EVERY 16   /* callbacks run between pool returns and progress updates */
#endif
#ifndef CB_TAKE
#define CB_TAKE 1          /* nodes a worker takes from the ready stack at once */
#endif
#ifndef CB_STALE_US
#define CB_STALE_US 20
#endif

/* worker state word: sleep epoch << 2 | state */
#define CB_AWAKE    0
#define CB_SLEEPING 1
#define CB_CLAIMED  2
#define CB_STATE(e, st) (((size_t)(e) << 2) | (st))
#define CB_ST(s)        ((s) & 3)
#define CB_EPOCH(s)     ((s) >> 2)
#define CB_MAX_WORKERS (8 * sizeof(size_t))

/* index of the lowest set bit of a non-zero mask */
#if defined(__GNUC__)
#  define CB_LOWBIT(m) ((unsigned)__builtin_ctzll(m))
#else
static unsigned cbLowBit(size_t m)
{
    unsigned i = 0;
    while (!(m & 1)) { m >>= 1; i++; }
    return i;
}
#  define CB_LOWBIT(m) cbLowBit(m)
#endif

/* each worker, and each shared word of a queue, on its own pair of
 * cache lines: adjacent-line prefetch would otherwise drag a line the
 * other side writes per callback along with one this side only reads */
#define CB_WORKER_ALIGN 128
#if defined(_MSC_VER)
#  define CB_ALIGN_PRE  __declspec(align(CB_WORKER_ALIGN))
#  define CB_ALIGN_POST
#elif defined(__GNUC__)
#  define CB_ALIGN_PRE
#  define CB_ALIGN_POST __attribute__((aligned(CB_WORKER_ALIGN)))
#else
#  define CB_ALIGN_PRE
#  define CB_ALIGN_POST
#endif

typedef struct cbNode {
    epicsCallback *cb;
    struct cbNode *next;
} cbNode;

/* free list head: node index + ABA tag packed in one size_t */
#if SIZE_MAX > 0xFFFFFFFFu
#  define CB_IDX_BITS 32
#else
#  define CB_IDX_BITS 16
#endif
#define CB_IDX_MASK   (((size_t)1 << CB_IDX_BITS) - 1)
#define CB_IDX_NONE   CB_IDX_MASK
#define CB_PACK(i, t) ((size_t)(i) | ((size_t)(t) << CB_IDX_BITS))
#define CB_IDX(h)     ((h) & CB_IDX_MASK)
#define CB_TAG(h)     ((h) >> CB_IDX_BITS)

typedef CB_ALIGN_PRE struct cbWorker {
    epicsEventId wake;
    size_t state;               /* atomic CB_STATE; AWAKE -> SLEEPING and
                                 * * -> AWAKE by the worker, SLEEPING ->
                                 * CLAIMED by a waker */
    int busy;                   /* atomic: inside a callback */
    epicsThreadId tid;
    unsigned idx;
} CB_ALIGN_POST cbWorker;

/* the shared words on their own cache-line pairs, grouped by who
 * writes them: the inbox and the free list by requesters, the counters
 * by both, the ready stack by workers per take */
typedef CB_ALIGN_PRE struct cbQueueSet {
    EpicsAtomicPtrT inbox;  /* cbNode*, newest first */
    char pad0[CB_WORKER_ALIGN - sizeof(EpicsAtomicPtrT)];
    size_t freeHead;        /* atomic, CB_PACK(index, tag) */
    char pad1[CB_WORKER_ALIGN - sizeof(size_t)];
    int nQueued;            /* atomic: nodes taken but not yet run */
    int maxQueued;          /* atomic, racy high-water mark */
    int queueOverflows;
    int batches;            /* atomic: progress by any worker */
    int lastBatches;        /* atomic: batches as last seen by a requester
                             * behind a backlog */
    size_t staleSince;      /* atomic: when (us) a requester first saw that
                             * value behind a backlog, 0 if none */
    char pad2[CB_WORKER_ALIGN - 5 * sizeof(int) - sizeof(size_t)];
    size_t ready;           /* atomic, CB_PACK(index, tag): nodes taken
                             * from the inbox, oldest first */
    char pad3[CB_WORKER_ALIGN - sizeof(size_t)];
    size_t sleepers;        /* atomic bitmask hint: SLEEPING workers nobody
                             * has taken yet; set by the worker, cleared by
                             * its taker or the worker */
    int nAwake;             /* atomic: workers not SLEEPING (incl. CLAIMED);
                             * lags the claim CAS, so it can read below 0 */
    char pad4[CB_WORKER_ALIGN - sizeof(size_t) - sizeof(int)];
    cbNode *pool;
    int shutdown; // use atomic
    int threadsConfigured;
    int threadsRunning;
    cbWorker *workers;
    void *workersRaw;       /* allocation behind the aligned workers array */
} CB_ALIGN_POST cbQueueSet;

static cbQueueSet callbackQueue[NUM_CALLBACK_PRIORITIES];

int callbackThreadsDefault = 1;
/* Don't know what a reasonable default is (yet).
 * For the time being: parallel means 2 if not explicitly specified */
int callbackParallelThreadsDefault = 2;
epicsExportAddress(int,callbackParallelThreadsDefault);

/* Timer for Delayed Requests */
static epicsTimerQueueId timerQueue;

enum cbState_t {
    cbInit,  /* before callbackInit() and after callbackCleanup() */
    cbRun,   /* after callbackInit() and before callbackStop() */
    cbStop,  /* after callbackStop() and before callbackCleanup() */
};

static int cbState; // holdscbState_t, use atomic ops

static epicsEventId startStopEvent;

/* Static data */
static const char *threadNamePrefix[NUM_CALLBACK_PRIORITIES] = {
    "cbLow", "cbMedium", "cbHigh"
};
#define FULL_MSG(name) "callbackRequest: " ERL_ERROR " " name " ring buffer full\n"
static const char *fullMessage[NUM_CALLBACK_PRIORITIES] = {
    FULL_MSG("cbLow"), FULL_MSG("cbMedium"), FULL_MSG("cbHigh")
};
static const unsigned int threadPriority[NUM_CALLBACK_PRIORITIES] = {
    epicsThreadPriorityScanLow - 1,
    epicsThreadPriorityScanLow + 4,
    epicsThreadPriorityScanHigh + 1
};


int callbackSetQueueSize(int size)
{
    if (size<=0) {
        fprintf(stderr, "Queue size must be positive\n");
        return -1;
    }
    if ((size_t)size >= CB_IDX_NONE) {
        fprintf(stderr, "Queue size must be below %lu\n",
            (unsigned long)CB_IDX_NONE);
        return -1;
    }
    if (epicsAtomicGetIntT(&cbState)!=cbInit) {
        fprintf(stderr, "Callback system already initialized\n");
        return -1;
    }
    callbackQueueSize = size;
    return 0;
}

int callbackQueueStatus(const int reset, callbackQueueStats *result)
{
    int ret;
    if (epicsAtomicGetIntT(&cbState)==cbInit) return -1;
    if (result) {
        int prio;
        result->size = callbackQueueSize;
        for(prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            cbQueueSet *mySet = &callbackQueue[prio];
            result->numUsed[prio] = epicsAtomicGetIntT(&mySet->nQueued);
            result->maxUsed[prio] = epicsAtomicGetIntT(&mySet->maxQueued);
            result->numOverflow[prio] = epicsAtomicGetIntT(&mySet->queueOverflows);
        }
        ret = 0;
    } else {
        ret = -2;
    }
    if (reset) {
        int prio;
        for(prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            epicsAtomicSetIntT(&callbackQueue[prio].maxQueued, 0);
        }
    }
    return ret;
}

void callbackQueueShow(const int reset)
{
    callbackQueueStats stats;
    if (callbackQueueStatus(reset, &stats) == -1) {
        fprintf(stderr, "Callback system not initialized, yet. Please run "
            "iocInit before using this command.\n");
    } else {
        int prio;
        printf("PRIORITY  HIGH-WATER MARK  ITEMS IN Q  Q SIZE  %% USED  Q OVERFLOWS\n");
        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            double qusage = 100.0 * stats.numUsed[prio] / stats.size;
            printf("%8s  %15d  %10d  %6d  %6.1f  %11d\n",
                   threadNamePrefix[prio], stats.maxUsed[prio],
                   stats.numUsed[prio], stats.size, qusage,
                   stats.numOverflow[prio]);
        }
    }
}

int callbackParallelThreads(int count, const char *prio)
{
    if (epicsAtomicGetIntT(&cbState)!=cbInit) {
        fprintf(stderr, "Callback system already initialized\n");
        return -1;
    }

    if (count < 0)
        count = epicsThreadGetCPUs() + count;
    else if (count == 0)
        count = callbackParallelThreadsDefault;
    if (count < 1) count = 1;

    if (!prio || *prio == 0 || strcmp(prio, "*") == 0) {
        int i;

        for (i = 0; i < NUM_CALLBACK_PRIORITIES; i++) {
            callbackQueue[i].threadsConfigured = count;
        }
    }
    else {
        dbMenu *pdbMenu;
        int i;

        if (!pdbbase) {
            fprintf(stderr, "callbackParallelThreads: pdbbase not set\n");
            return -1;
        }

        /* Find prio in menuPriority */
        pdbMenu = dbFindMenu(pdbbase, "menuPriority");
        if (!pdbMenu) {
            fprintf(stderr, "callbackParallelThreads: No Priority menu\n");
            return -1;
        }

        for (i = 0; i < pdbMenu->nChoice; i++) {
            if (epicsStrCaseCmp(prio, pdbMenu->papChoiceValue[i]) == 0)
                goto found;
        }
        fprintf(stderr, "callbackParallelThreads: "
            "Unknown priority \"%s\"\n", prio);
        return -1;

found:
        callbackQueue[i].threadsConfigured = count;
    }
    return 0;
}

/* node pool: tagged lock-free LIFO; pop is per node, push is a chain */
static cbNode *nodeAlloc(cbQueueSet *mySet)
{
    for (;;) {
        size_t h = epicsAtomicGetSizeT(&mySet->freeHead);
        size_t i = CB_IDX(h);
        cbNode *n;
        size_t nx;
        if (i == CB_IDX_NONE) return NULL;
        n = &mySet->pool[i];
        nx = n->next ? (size_t)(n->next - mySet->pool) : CB_IDX_NONE;
        if (epicsAtomicCmpAndSwapSizeT(&mySet->freeHead, h, CB_PACK(nx, CB_TAG(h) + 1)) == h)
            return n;
    }
}

static void nodeFreeChain(cbQueueSet *mySet, cbNode *first, cbNode *last)
{
    for (;;) {
        size_t h = epicsAtomicGetSizeT(&mySet->freeHead);
        size_t i = CB_IDX(h);
        last->next = (i == CB_IDX_NONE) ? NULL : &mySet->pool[i];
        if (epicsAtomicCmpAndSwapSizeT(&mySet->freeHead, h,
                CB_PACK(first - mySet->pool, CB_TAG(h) + 1)) == h)
            return;
    }
}

/* ready stack: the same tagged LIFO; a take is up to CB_TAKE nodes
 * from the head, a push is a chain */
static void readyPush(cbQueueSet *mySet, cbNode *first, cbNode *last, size_t *ph)
{
    size_t h = *ph;
    for (;;) {
        size_t i = CB_IDX(h), cur, nh;
        last->next = (i == CB_IDX_NONE) ? NULL : &mySet->pool[i];
        nh = CB_PACK(first - mySet->pool, CB_TAG(h) + 1);
        cur = epicsAtomicCmpAndSwapSizeT(&mySet->ready, h, nh);
        if (cur == h) { *ph = nh; return; }
        h = cur;
    }
}

/* take up to CB_TAKE nodes from the head of the ready stack with one
 * CAS, as a NULL-terminated chain; *more says whether the stack holds
 * more. *ph is the head as this worker last left it: a CAS against it
 * needs no fresh read while nobody else touched the stack, and
 * returns the current head when somebody did. Walking the chain races
 * with other takers, but every next pointer leads into the pool or to
 * NULL and the tagged head rejects a chain that changed under us. */
static cbNode *readyTake(cbQueueSet *mySet, size_t *ph, int *more)
{
    size_t h = *ph;
    if (CB_IDX(h) == CB_IDX_NONE)
        h = epicsAtomicGetSizeT(&mySet->ready);
    for (;;) {
        size_t i = CB_IDX(h), nx, cur, nh;
        cbNode *n, *last;
        int k = 1;
        if (i == CB_IDX_NONE) { *ph = h; *more = 0; return NULL; }
        n = last = &mySet->pool[i];
        while (k < CB_TAKE && last->next) { last = last->next; k++; }
        nx = last->next ? (size_t)(last->next - mySet->pool) : CB_IDX_NONE;
        nh = CB_PACK(nx, CB_TAG(h) + 1);
        cur = epicsAtomicCmpAndSwapSizeT(&mySet->ready, h, nh);
        if (cur == h) {
            last->next = NULL;
            *ph = nh;
            *more = nx != CB_IDX_NONE;
            return n;
        }
        h = cur;
    }
}

static int readyEmpty(cbQueueSet *mySet)
{
    return CB_IDX(epicsAtomicGetSizeT(&mySet->ready)) == CB_IDX_NONE;
}

/* take the whole inbox and reverse it to oldest first; *plast is its
 * last node */
static cbNode *grabInbox(cbQueueSet *mySet, cbNode **plast)
{
    cbNode *head, *rev = NULL;

    do {
        head = epicsAtomicGetPtrT(&mySet->inbox);
        if (!head) return NULL;
    } while (epicsAtomicCmpAndSwapPtrT(&mySet->inbox, head, NULL) != head);
    *plast = head;
    while (head) {
        cbNode *nx = head->next;
        head->next = rev;
        rev = head;
        head = nx;
    }
    return rev;
}

/* the next nodes to run, up to CB_TAKE as a NULL-terminated chain:
 * from the ready stack, else the oldest of the inbox, whose rest goes
 * onto the stack. *more says whether the stack holds work after this */
static cbNode *nextBatch(cbQueueSet *mySet, size_t *ph, int *more)
{
    cbNode *nd = readyTake(mySet, ph, more), *last, *cut;
    int k = 1;

    if (nd) return nd;
    nd = grabInbox(mySet, &last);
    if (!nd) return NULL;   /* *more is 0 from the take */
    cut = nd;
    while (k < CB_TAKE && cut->next) { cut = cut->next; k++; }
    *more = cut->next != NULL;
    if (cut->next) {
        readyPush(mySet, cut->next, last, ph);
        cut->next = NULL;
    }
    return nd;
}

/* clear worker i's hint bit; returns whether this call cleared it */
static int takeSleeperBit(cbQueueSet *mySet, unsigned i)
{
    size_t bit = (size_t)1 << i;
    for (;;) {
        size_t m = epicsAtomicGetSizeT(&mySet->sleepers);
        if (!(m & bit)) return 0;
        if (epicsAtomicCmpAndSwapSizeT(&mySet->sleepers, m, m & ~bit) == m) return 1;
    }
}

/* trigger a sleeping worker, then claim it (SLEEPING -> CLAIMED of the
 * same epoch) so it is counted awake. A failed claim means the worker
 * left that sleep by itself and counted itself; the trigger then at
 * worst wakes its next sleep once for nothing. */
static void triggerAndClaim(cbQueueSet *mySet, cbWorker *w, size_t s)
{
    epicsEventMustTrigger(w->wake);
    if (epicsAtomicCmpAndSwapSizeT(&w->state, s, CB_STATE(CB_EPOCH(s), CB_CLAIMED)) != s)
        return;
    epicsAtomicIncrIntT(&mySet->nAwake);
}

/* wake one sleeping worker. One trigger is enough even when the claim
 * fails: the worker then left its sleep by itself after the request
 * was pushed and reads the queue next. (A retry would never end on
 * one core where the worker preempts the trigger, runs, and sleeps
 * again before the claim.) Tries the hinted workers first, then every
 * worker once if nAwake says one sleeps: a waker stopped after taking
 * a bit leaves none. A worker found in neither pass is leaving its
 * sleep and reads the queue. */
static void pokeSleeper(cbQueueSet *mySet)
{
    size_t m = epicsAtomicGetSizeT(&mySet->sleepers);
    int i;

    while (m) {
        i = CB_LOWBIT(m);
        m &= ~((size_t)1 << i);
        if (!takeSleeperBit(mySet, i)) continue;
        {
            cbWorker *w = &mySet->workers[i];
            size_t s = epicsAtomicGetSizeT(&w->state);
            if (CB_ST(s) == CB_SLEEPING) {
                triggerAndClaim(mySet, w, s);
                return;
            }
        }
        /* its bit was stale: that worker is leaving its sleep; scan again */
        m = epicsAtomicGetSizeT(&mySet->sleepers);
    }
    if (epicsAtomicGetIntT(&mySet->nAwake) < mySet->threadsConfigured) {
        for (i = 0; i < mySet->threadsConfigured; i++) {
            cbWorker *w = &mySet->workers[i];
            size_t s = epicsAtomicGetSizeT(&w->state);
            if (CB_ST(s) == CB_SLEEPING) {
                triggerAndClaim(mySet, w, s);
                return;
            }
        }
    }
}

/* whether some worker is neither sleeping nor inside a callback, so
 * will read the inbox before its next callback or sleep */
static int anyReady(cbQueueSet *mySet)
{
    int j;

    for (j = 0; j < mySet->threadsConfigured; j++) {
        cbWorker *w = &mySet->workers[j];
        if (CB_ST(epicsAtomicGetSizeT(&w->state)) != CB_SLEEPING &&
            !epicsAtomicGetIntT(&w->busy))
            return 1;
    }
    return 0;
}

static void callbackTask(void *arg)
{
    cbWorker *me = arg;
    cbQueueSet *mySet = &callbackQueue[me->idx >> 8];
    size_t mybit = (size_t)1 << (me->idx & 0xff);
    size_t epoch = 0;
    size_t ready = CB_PACK(CB_IDX_NONE, 0);   /* the stack head as last seen */
    cbNode *done = NULL, *doneLast = NULL;  /* ran, not yet returned to the pool */
    int ran = 0;

    taskwdInsert(0, NULL, NULL);
    epicsEventSignal(startStopEvent);

    while(!epicsAtomicGetIntT(&mySet->shutdown)) {
        cbNode *nd;
        int more;

        /* busy first, then the queue and the sleepers: a request pushed
         * after this finds us busy and wakes a sleeper itself; one
         * pushed before, and the rest of the queue, is seen here and a
         * sleeper woken for it */
        epicsAtomicSetIntT(&me->busy, 1);
        nd = nextBatch(mySet, &ready, &more);
        while (nd) {
            epicsCallback *cb = nd->cb;
            cbNode *nx = nd->next;

            if (epicsAtomicGetSizeT(&mySet->sleepers) &&
                (more || !readyEmpty(mySet) || epicsAtomicGetPtrT(&mySet->inbox)))
                pokeSleeper(mySet);
            (*cb->callback)(cb);
            epicsAtomicSetIntT(&me->busy, 0);
            nd->next = done;
            done = nd;
            if (!doneLast) doneLast = nd;
            if (++ran == CB_FREE_EVERY) {
                nodeFreeChain(mySet, done, doneLast);
                done = doneLast = NULL;
                epicsAtomicAddIntT(&mySet->nQueued, -ran);
                epicsAtomicIncrIntT(&mySet->batches);
                ran = 0;
            }
            nd = nx;
            if (nd) {
                epicsAtomicSetIntT(&me->busy, 1);
                more = 0;
            }
        }
        if (more) continue;
        epicsAtomicSetIntT(&me->busy, 0);
        if (ran) {
            nodeFreeChain(mySet, done, doneLast);
            done = doneLast = NULL;
            epicsAtomicAddIntT(&mySet->nQueued, -ran);
            epicsAtomicIncrIntT(&mySet->batches);
            ran = 0;
        }

        /* announce sleep with a fresh epoch (state, hint bit, count),
         * then look at the inbox and the stack: a request pushed
         * before this point is seen here; one after it sees nAwake==0
         * or our bit, and our SLEEPING state behind it.
         * Leaving the sleep by ourselves counts us awake; if a waker
         * claimed us first, it counted us and its trigger stays stored
         * in the event. */
        {
            size_t sleeping = CB_STATE(++epoch, CB_SLEEPING);
            epicsAtomicSetSizeT(&me->state, sleeping);
            for (;;) {
                size_t m = epicsAtomicGetSizeT(&mySet->sleepers);
                if (epicsAtomicCmpAndSwapSizeT(&mySet->sleepers, m, m | mybit) == m) break;
            }
            epicsAtomicDecrIntT(&mySet->nAwake);
            if (epicsAtomicGetPtrT(&mySet->inbox) == NULL && readyEmpty(mySet))
                epicsEventMustWait(me->wake);
            if (epicsAtomicCmpAndSwapSizeT(&me->state, sleeping,
                    CB_STATE(epoch, CB_AWAKE)) == sleeping)
                epicsAtomicIncrIntT(&mySet->nAwake);
            else
                epicsAtomicSetSizeT(&me->state, CB_STATE(epoch, CB_AWAKE));
            takeSleeperBit(mySet, me->idx & 0xff);
            ready = CB_PACK(CB_IDX_NONE, 0);
        }
    }
    if (ran) {
        nodeFreeChain(mySet, done, doneLast);
        epicsAtomicAddIntT(&mySet->nQueued, -ran);
    }

    if(!epicsAtomicDecrIntT(&mySet->threadsRunning))
        epicsEventSignal(startStopEvent);
    taskwdRemove(0);
}

void callbackStop(void)
{
    int i;

    if (epicsAtomicCmpAndSwapIntT(&cbState, cbRun, cbStop)!=cbRun) return;

    for (i = 0; i < NUM_CALLBACK_PRIORITIES; i++) {
        cbQueueSet *mySet = &callbackQueue[i];
        int j;

        epicsAtomicSetIntT(&mySet->shutdown, 1);
        while (epicsAtomicGetIntT(&mySet->threadsRunning)) {
            for(j=0; j<mySet->threadsConfigured; j++)
                epicsEventSignal(mySet->workers[j].wake);
            epicsEventWaitWithTimeout(startStopEvent, 0.1);
        }
        for(j=0; j<mySet->threadsConfigured; j++) {
            epicsThreadMustJoin(mySet->workers[j].tid);
        }
    }
}

void callbackCleanup(void)
{
    int i;

    if(epicsAtomicCmpAndSwapIntT(&cbState, cbStop, cbInit)!=cbStop) {
        fprintf(stderr, "callbackCleanup() but not stopped\n");
    }

    for (i = 0; i < NUM_CALLBACK_PRIORITIES; i++) {
        cbQueueSet *mySet = &callbackQueue[i];

        int j;

        assert(epicsAtomicGetIntT(&mySet->threadsRunning)==0);
        for(j=0; j<mySet->threadsConfigured; j++) {
            epicsEventDestroy(mySet->workers[j].wake);
        }
        free(mySet->workersRaw);
        mySet->workers = NULL;
        mySet->workersRaw = NULL;
        free(mySet->pool);
        mySet->pool = NULL;
    }

    epicsTimerQueueRelease(timerQueue);
    memset(callbackQueue, 0, sizeof(callbackQueue));
}

void callbackInit(void)
{
    int i;
    int j;
    char threadName[32];

    if (epicsAtomicCmpAndSwapIntT(&cbState, cbInit, cbRun)!=cbInit) {
        fprintf(stderr, "Warning: callbackInit called again before callbackCleanup\n");
        return;
    }

    if(!startStopEvent)
        startStopEvent = epicsEventMustCreate(epicsEventEmpty);

    timerQueue = epicsTimerQueueAllocate(0, epicsThreadPriorityScanHigh);

    for (i = 0; i < NUM_CALLBACK_PRIORITIES; i++) {
        epicsThreadId tid;

        {
            cbQueueSet *q = &callbackQueue[i];
            int k;
            q->pool = callocMustSucceed(callbackQueueSize, sizeof(*q->pool), "callbackInit");
            for (k = 0; k < callbackQueueSize - 1; k++)
                q->pool[k].next = &q->pool[k + 1];
            q->pool[callbackQueueSize - 1].next = NULL;
            q->freeHead = CB_PACK(0, 0);
            q->ready = CB_PACK(CB_IDX_NONE, 0);
        }
        callbackQueue[i].inbox = NULL;
        callbackQueue[i].nQueued = 0;
        callbackQueue[i].maxQueued = 0;
        callbackQueue[i].sleepers = 0;

        if (callbackQueue[i].threadsConfigured == 0)
            callbackQueue[i].threadsConfigured = callbackThreadsDefault;
        if (callbackQueue[i].threadsConfigured > (int)CB_MAX_WORKERS) {
            errlogPrintf("callbackInit: %d %s threads requested, using the limit of %d\n",
                callbackQueue[i].threadsConfigured, threadNamePrefix[i],
                (int)CB_MAX_WORKERS);
            callbackQueue[i].threadsConfigured = CB_MAX_WORKERS;
        }

        callbackQueue[i].workersRaw = callocMustSucceed(1,
            callbackQueue[i].threadsConfigured * sizeof(*callbackQueue[i].workers) + CB_WORKER_ALIGN,
            "callbackInit");
        callbackQueue[i].workers = (cbWorker *)(((uintptr_t)callbackQueue[i].workersRaw +
                                                 CB_WORKER_ALIGN - 1) & ~(uintptr_t)(CB_WORKER_ALIGN - 1));
        /* workers start awake; each goes to sleep once it finds the queue empty */
        callbackQueue[i].nAwake = callbackQueue[i].threadsConfigured;

        for (j = 0; j < callbackQueue[i].threadsConfigured; j++) {
            cbWorker *w = &callbackQueue[i].workers[j];
            epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
            opts.joinable = 1;
            opts.priority = threadPriority[i];
            opts.stackSize = epicsThreadStackBig;
            if (callbackQueue[i].threadsConfigured > 1 )
                sprintf(threadName, "%s-%d", threadNamePrefix[i], j);
            else
                strcpy(threadName, threadNamePrefix[i]);
            w->idx = (i << 8) | j;
            w->wake = epicsEventMustCreate(epicsEventEmpty);
            w->tid = tid = epicsThreadCreateOpt(threadName,
                (EPICSTHREADFUNC)callbackTask, w, &opts);
            if (tid == 0) {
                cantProceed("Failed to spawn callback thread %s\n", threadName);
            } else {
                epicsEventWait(startStopEvent);
                epicsAtomicIncrIntT(&callbackQueue[i].threadsRunning);
            }
        }
    }
}

/* This routine can be called from interrupt context */
int callbackRequest(epicsCallback *pcallback)
{
    int priority;
    int n;
    cbNode *node;
    cbQueueSet *mySet;

    if (!pcallback) {
        epicsInterruptContextMessage("callbackRequest: " ERL_ERROR " pcallback was NULL\n");
        return S_db_notInit;
    }
    if (!pcallback->callback) {
        epicsInterruptContextMessage("callbackRequest: " ERL_ERROR " pcallback->callback was NULL\n");
        return S_db_notInit;
    }
    priority = pcallback->priority;
    if (priority < 0 || priority >= NUM_CALLBACK_PRIORITIES) {
        epicsInterruptContextMessage("callbackRequest: " ERL_ERROR " Bad priority\n");
        return S_db_badChoice;
    }
    mySet = &callbackQueue[priority];
    if (!mySet->workers) {
        epicsInterruptContextMessage("callbackRequest: " ERL_ERROR " Callbacks not initialized\n");
        return S_db_notInit;
    }

    node = nodeAlloc(mySet);
    if (!node) {
        epicsInterruptContextMessage(fullMessage[priority]);
        epicsAtomicIncrIntT(&mySet->queueOverflows);
        return S_db_bufFull;
    }
    node->cb = pcallback;
    n = epicsAtomicIncrIntT(&mySet->nQueued);
    if (n > epicsAtomicGetIntT(&mySet->maxQueued))
        epicsAtomicSetIntT(&mySet->maxQueued, n);

    {
        cbNode *old;
        int awake;
        do {
            old = epicsAtomicGetPtrT(&mySet->inbox);
            node->next = old;
        } while (epicsAtomicCmpAndSwapPtrT(&mySet->inbox, old, node) != old);

        /* a worker that is awake and not inside a callback will take
         * this; recruiting more is its job. If every awake worker is
         * inside a callback while one sleeps, wake it now; that check
         * is owed only when the inbox was empty, as a request already
         * there has a worker on its way whose take-all covers ours.
         * And if only one of several workers is counted awake and for
         * CB_STALE_US no worker made progress, that worker is
         * stopped, so wake a sleeper now. nAwake lags its claim CAS: a
         * claimer stopped between the claim and its increment lets the
         * claimed worker run and sleep first, so the count can read
         * below zero; anything non-positive means nobody is counted. */
        awake = epicsAtomicGetIntT(&mySet->nAwake);
        if (awake <= 0)
            pokeSleeper(mySet);
        else if (old == NULL && awake < mySet->threadsConfigured
                 && epicsAtomicGetSizeT(&mySet->sleepers) && !anyReady(mySet))
            pokeSleeper(mySet);
        else if (awake == 1 && mySet->threadsConfigured > 1) {
            int b = epicsAtomicGetIntT(&mySet->batches);
            if (b != epicsAtomicGetIntT(&mySet->lastBatches)) {
                epicsAtomicSetIntT(&mySet->lastBatches, b);
                epicsAtomicSetSizeT(&mySet->staleSince, 0);
            }
            else {
                size_t now = (size_t)(epicsMonotonicGet() >> 10) | 1;
                size_t since = epicsAtomicGetSizeT(&mySet->staleSince);
                if (since == 0)
                    epicsAtomicSetSizeT(&mySet->staleSince, now);
                else if (now - since >= CB_STALE_US) {
                    epicsAtomicSetSizeT(&mySet->staleSince, 0);
                    pokeSleeper(mySet);
                }
            }
        }
    }
    return 0;
}

static void ProcessCallback(epicsCallback *pcallback)
{
    dbCommon *pRec;

    callbackGetUser(pRec, pcallback);
    if (!pRec) return;
    dbScanLock(pRec);
    (*pRec->rset->process)(pRec);
    dbScanUnlock(pRec);
}

void callbackSetProcess(epicsCallback *pcallback, int Priority, void *pRec)
{
    callbackSetCallback(ProcessCallback, pcallback);
    callbackSetPriority(Priority, pcallback);
    callbackSetUser(pRec, pcallback);
}

int  callbackRequestProcessCallback(epicsCallback *pcallback,
    int Priority, void *pRec)
{
    callbackSetProcess(pcallback, Priority, pRec);
    return callbackRequest(pcallback);
}

static void notify(void *pPrivate)
{
    epicsCallback *pcallback = (epicsCallback *)pPrivate;
    callbackRequest(pcallback);
}

void callbackRequestDelayed(epicsCallback *pcallback, double seconds)
{
    epicsTimerId timer = (epicsTimerId)pcallback->timer;

    if (timer == 0) {
        timer = epicsTimerQueueCreateTimer(timerQueue, notify, pcallback);
        pcallback->timer = timer;
    }
    epicsTimerStartDelay(timer, seconds);
}

void callbackCancelDelayed(epicsCallback *pcallback)
{
    epicsTimerId timer = (epicsTimerId)pcallback->timer;

    if (timer != 0) {
        epicsTimerCancel(timer);
    }
}

void callbackRequestProcessCallbackDelayed(epicsCallback *pcallback,
    int Priority, void *pRec, double seconds)
{
    callbackSetProcess(pcallback, Priority, pRec);
    callbackRequestDelayed(pcallback, seconds);
}

/* Sync. process of testSyncCallback()
 *
 * 1. For each priority, make a call to callbackRequest() for each worker.
 * 2. Wait until all callbacks are concurrently being executed
 * 3. Last worker to begin executing signals success and begins waking up other workers
 * 4. Last worker to wake signals testSyncCallback() to complete
 */
typedef struct {
    epicsEventId wait_phase2, wait_phase4;
    int nphase2, nphase3;
    epicsCallback cb;
} sync_helper;

static void sync_callback(epicsCallback *cb)
{
    sync_helper *helper;
    callbackGetUser(helper, cb);

    testGlobalLock();

    assert(helper->nphase2 > 0);
    if(--helper->nphase2!=0) {
        /* we are _not_ the last to start. */
        testGlobalUnlock();
        epicsEventMustWait(helper->wait_phase2);
        testGlobalLock();
    }

    /* we are either the last to start, or have been
     * woken by the same and must pass the wakeup along
     */
    epicsEventMustTrigger(helper->wait_phase2);

    assert(helper->nphase2 == 0);
    assert(helper->nphase3 > 0);

    if(--helper->nphase3==0) {
        /* we are the last to wake up.  wake up testSyncCallback() */
        epicsEventMustTrigger(helper->wait_phase4);
    }

    testGlobalUnlock();
}

void testSyncCallback(void)
{
    sync_helper helper[NUM_CALLBACK_PRIORITIES];
    unsigned i;

    testDiag("Begin testSyncCallback()");

    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
        helper[i].wait_phase2 = epicsEventMustCreate(epicsEventEmpty);
        helper[i].wait_phase4 = epicsEventMustCreate(epicsEventEmpty);

        /* no real need to lock here, but do so anyway so that valgrind can establish
         * the locking requirements for sync_helper.
         */
        testGlobalLock();
        helper[i].nphase2 = helper[i].nphase3 = callbackQueue[i].threadsRunning;
        testGlobalUnlock();

        callbackSetUser(&helper[i], &helper[i].cb);
        callbackSetPriority(i, &helper[i].cb);
        callbackSetCallback(sync_callback, &helper[i].cb);

        callbackRequest(&helper[i].cb);
    }

    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
        epicsEventMustWait(helper[i].wait_phase4);
    }

    for(i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
        testGlobalLock();
        epicsEventDestroy(helper[i].wait_phase2);
        epicsEventDestroy(helper[i].wait_phase4);
        testGlobalUnlock();
    }

    testDiag("Complete testSyncCallback()");
}
