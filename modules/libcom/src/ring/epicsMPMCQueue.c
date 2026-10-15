/*************************************************************************\
* Copyright (c) 2026 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Bounded MPMC queue after Dmitry Vyukov: each cell has a sequence number
 * that tells producers and consumers which lap of the ring it belongs to.
 * Only compare-and-swap is relied upon for ordering (it is a full barrier
 * for both CPU and compiler on all targets), not the memory barriers.
 */

#include <stddef.h>
#include <stdlib.h>

#include "epicsAssert.h"
#include "epicsAtomic.h"
#include "epicsMPMCQueue.h"

#define PAD 128
#define MAX_SIZE (1 << 30)

STATIC_ASSERT(sizeof(ptrdiff_t) == sizeof(size_t));

typedef struct {
    size_t seq;
    void *data;
} cell;

struct epicsMPMCQueue {
    cell *buf;
    size_t mask;
    char pad1[PAD];
    size_t enq;
    size_t hwm;
    char pad2[PAD];
    size_t deq;
    char pad3[PAD];
};

epicsMPMCQueueId epicsStdCall epicsMPMCQueueCreate(int size)
{
    struct epicsMPMCQueue *q;
    size_t n = 2, i;

    if (size <= 0 || size > MAX_SIZE)
        return NULL;
    while (n < (size_t)size)
        n <<= 1;

    q = calloc(1, sizeof(*q));
    if (!q)
        return NULL;
    q->buf = calloc(n, sizeof(cell));
    if (!q->buf) {
        free(q);
        return NULL;
    }
    q->mask = n - 1;
    /* Start just below wrap-around, so that it is always exercised */
    q->enq = q->deq = (size_t)0 - 2 * n;
    for (i = 0; i < n; i++)
        q->buf[i].seq = q->enq + i;
    return q;
}

void epicsStdCall epicsMPMCQueueDelete(epicsMPMCQueueId q)
{
    if (q) {
        free(q->buf);
        free(q);
    }
}

/* Fresh value of *p, also where the barriers are empty (vxWorks < 6.6) */
static size_t reload(size_t *p, size_t guess)
{
    return epicsAtomicCmpAndSwapSizeT(p, guess, guess);
}

static size_t usedAt(epicsMPMCQueueId q, size_t enq)
{
    size_t used = enq - epicsAtomicGetSizeT(&q->deq);

    return used > q->mask + 1 ? 0 : used;
}

int epicsStdCall epicsMPMCQueuePush(epicsMPMCQueueId q, void *p)
{
    size_t pos = epicsAtomicGetSizeT(&q->enq);
    size_t used, hwm;
    cell *c;

    if (!p)
        return 0;
    for (;;) {
        ptrdiff_t diff;

        c = &q->buf[pos & q->mask];
        diff = (ptrdiff_t)(epicsAtomicGetSizeT(&c->seq) - pos);
        if (diff == 0) {
            size_t old = epicsAtomicCmpAndSwapSizeT(&q->enq, pos, pos + 1);
            if (old == pos)
                break;
            pos = old;
        } else if (diff < 0) {
            return 0;
        } else {
            pos = reload(&q->enq, pos);
        }
    }
    c->data = p;
    epicsAtomicCmpAndSwapSizeT(&c->seq, pos, pos + 1);

    used = usedAt(q, pos + 1);
    hwm = epicsAtomicGetSizeT(&q->hwm);
    while (used > hwm) {
        size_t old = epicsAtomicCmpAndSwapSizeT(&q->hwm, hwm, used);
        if (old == hwm)
            break;
        hwm = old;
    }
    return 1;
}

void* epicsStdCall epicsMPMCQueuePop(epicsMPMCQueueId q)
{
    size_t pos = epicsAtomicGetSizeT(&q->deq);
    cell *c;
    void *p;

    for (;;) {
        ptrdiff_t diff;

        c = &q->buf[pos & q->mask];
        diff = (ptrdiff_t)(epicsAtomicGetSizeT(&c->seq) - (pos + 1));
        if (diff == 0) {
            size_t old = epicsAtomicCmpAndSwapSizeT(&q->deq, pos, pos + 1);
            if (old == pos)
                break;
            pos = old;
        } else if (diff < 0) {
            return NULL;
        } else {
            pos = reload(&q->deq, pos);
        }
    }
    p = c->data;
    epicsAtomicCmpAndSwapSizeT(&c->seq, pos + 1, pos + q->mask + 1);
    return p;
}

int epicsStdCall epicsMPMCQueueIsEmpty(epicsMPMCQueueId q)
{
    size_t pos = epicsAtomicGetSizeT(&q->deq);
    size_t seq = epicsAtomicGetSizeT(&q->buf[pos & q->mask].seq);

    return (ptrdiff_t)(seq - (pos + 1)) < 0;
}

int epicsStdCall epicsMPMCQueueGetSize(epicsMPMCQueueId q)
{
    return (int)(q->mask + 1);
}

int epicsStdCall epicsMPMCQueueGetUsed(epicsMPMCQueueId q)
{
    return (int)usedAt(q, epicsAtomicGetSizeT(&q->enq));
}

int epicsStdCall epicsMPMCQueueGetHighWaterMark(epicsMPMCQueueId q)
{
    return (int)epicsAtomicGetSizeT(&q->hwm);
}

void epicsStdCall epicsMPMCQueueResetHighWaterMark(epicsMPMCQueueId q)
{
    epicsAtomicSetSizeT(&q->hwm, usedAt(q, epicsAtomicGetSizeT(&q->enq)));
}
