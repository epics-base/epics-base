/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "epicsLifo.h"
#include "epicsAtomic.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct {
    epicsLifoNode node;
    unsigned producer;
    unsigned seq;
} item;

static void testSingleThread(void)
{
    epicsLifo lifo = EPICS_LIFO_INIT;
    item a, b, c, x, y, z;
    epicsLifoNode *n;

    testDiag("Single thread semantics");

    testOk1(epicsLifoIsEmpty(&lifo));
    testOk1(epicsLifoTakeAll(&lifo) == NULL);
    testOk1(epicsLifoReverse(NULL) == NULL);

    testOk(epicsLifoPush(&lifo, &a.node), "first push sees empty list");
    testOk(!epicsLifoPush(&lifo, &b.node), "second push sees non-empty list");
    testOk(!epicsLifoPush(&lifo, &c.node), "third push sees non-empty list");
    testOk1(!epicsLifoIsEmpty(&lifo));

    n = epicsLifoTakeAll(&lifo);
    testOk(n == &c.node && n->next == &b.node && n->next->next == &a.node
           && a.node.next == NULL, "take all returns newest first");
    testOk1(epicsLifoIsEmpty(&lifo));
    testOk1(epicsLifoTakeAll(&lifo) == NULL);

    n = epicsLifoReverse(n);
    testOk(n == &a.node && n->next == &b.node && n->next->next == &c.node
           && c.node.next == NULL, "reverse gives push order");

    x.node.next = &y.node;
    y.node.next = &a.node; /* must be overwritten */
    testOk(epicsLifoPushBatch(&lifo, &x.node, &y.node),
           "batch push onto empty list");
    testOk(!epicsLifoPush(&lifo, &z.node), "push after batch");
    n = epicsLifoTakeAll(&lifo);
    testOk(n == &z.node && n->next == &x.node && n->next->next == &y.node
           && y.node.next == NULL, "batch stays in order");

    epicsLifoInit(&lifo);
    testOk1(epicsLifoIsEmpty(&lifo));
}

/* Producers push numbered items, one consumer takes them all and checks
 * that every item arrives exactly once, and in push order per producer.
 * With a small pool of items recycled through a second epicsLifo, nodes
 * are reused all the time, which would expose ABA problems.
 */
#define NPRODUCERS 4u
#define NPOOL 64u
#define NPERPRODUCER 200000u

typedef struct {
    epicsLifo work;
    epicsLifo pool;
    item items[NPOOL];
} shared;

typedef struct {
    shared *pShared;
    unsigned id;
} producerArg;

static void producer(void *raw)
{
    const producerArg *pArg = (const producerArg *) raw;
    shared *pShared = pArg->pShared;
    epicsLifoNode *stash = NULL;
    unsigned seq, spin = 0u;

    for (seq = 0u; seq < NPERPRODUCER; seq++) {
        item *pItem;

        while (!stash) {
            stash = epicsLifoTakeAll(&pShared->pool);
            if (!stash && ++spin % 256u == 0u)
                epicsThreadSleep(0.0);
        }
        pItem = (item *) stash;
        stash = stash->next;

        pItem->producer = pArg->id;
        pItem->seq = seq;
        epicsLifoPush(&pShared->work, &pItem->node);
    }
    /* return what is left */
    while (stash) {
        epicsLifoNode *next = stash->next;
        epicsLifoPush(&pShared->pool, stash);
        stash = next;
    }
}

static void testMultiThread(void)
{
    shared *pShared = calloc(1, sizeof(*pShared));
    producerArg args[NPRODUCERS];
    epicsThreadId tids[NPRODUCERS];
    unsigned next[NPRODUCERS];
    unsigned long received = 0u, batches = 0u, bad = 0u;
    unsigned spin = 0u;
    const unsigned long total = (unsigned long) NPRODUCERS * NPERPRODUCER;
    unsigned i;

    testDiag("%u producers, %u items each, pool of %u items",
             NPRODUCERS, NPERPRODUCER, NPOOL);

    if (!pShared)
        testAbort("Out of memory");
    epicsLifoInit(&pShared->work);
    epicsLifoInit(&pShared->pool);
    for (i = 0u; i < NPOOL; i++)
        epicsLifoPush(&pShared->pool, &pShared->items[i].node);

    for (i = 0u; i < NPRODUCERS; i++) {
        epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
        opts.joinable = 1;
        opts.stackSize = epicsThreadStackSmall;
        args[i].pShared = pShared;
        args[i].id = i;
        next[i] = 0u;
        tids[i] = epicsThreadCreateOpt("producer", producer, &args[i], &opts);
        if (!tids[i])
            testAbort("Failed to create thread");
    }

    while (received < total) {
        epicsLifoNode *n;

        if (epicsLifoIsEmpty(&pShared->work)) {
            if (++spin % 256u == 0u)
                epicsThreadSleep(0.0);
            continue;
        }
        n = epicsLifoReverse(epicsLifoTakeAll(&pShared->work));
        if (!n)
            continue;
        batches++;
        while (n) {
            item *pItem = (item *) n;
            epicsLifoNode *pNext = n->next;

            if (pItem->producer >= NPRODUCERS ||
                pItem->seq != next[pItem->producer]) {
                if (bad++ < 10u)
                    testDiag("unexpected item producer %u seq %u",
                             pItem->producer, pItem->seq);
            }
            else {
                next[pItem->producer]++;
            }
            received++;
            epicsLifoPush(&pShared->pool, n);
            n = pNext;
        }
    }

    for (i = 0u; i < NPRODUCERS; i++)
        epicsThreadMustJoin(tids[i]);

    testDiag("%lu items in %lu batches", received, batches);
    testOk(bad == 0u, "all items in order per producer (%lu bad)", bad);
    testOk(received == total, "received %lu of %lu items", received, total);
    testOk1(epicsLifoIsEmpty(&pShared->work));

    received = 0u;
    {
        epicsLifoNode *n = epicsLifoTakeAll(&pShared->pool);
        for (; n; n = n->next)
            received++;
    }
    testOk(received == NPOOL, "all %lu pool items returned", received);

    free(pShared);
}

MAIN(epicsLifoTest)
{
    testPlan(19);
    testSingleThread();
#if defined(__rtems__)
    testSkip(4, "Test assumes time sliced thread scheduling");
#else
    testMultiThread();
#endif
    return testDone();
}
