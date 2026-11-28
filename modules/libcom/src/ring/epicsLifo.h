/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsLifo.h
 *
 * \brief A lock-free intrusive LIFO with push and take-all
 *
 * An epicsLifo is a singly linked stack of user-provided nodes that any
 * number of threads may push onto concurrently, while consumers take all
 * nodes at once. It is modelled on the Linux kernel's llist
 * ("lock-less list").
 *
 * - epicsLifoPush() and epicsLifoPushBatch() are lock-free: a pushing
 *   thread may have to retry its compare-and-swap when another push
 *   succeeds at the same time, but some thread always makes progress,
 *   and a thread that is preempted or suspended never blocks others.
 * - epicsLifoTakeAll() is wait-free: it detaches the whole list with
 *   a single atomic exchange.
 *
 * There is no operation to remove a single node. That would be
 * vulnerable to the ABA problem (a node that is popped and pushed again
 * while another thread is inside its compare-and-swap); taking all
 * nodes at once is not, so nodes may be reused immediately.
 *
 * The functions use no locks and do not allocate, so they can be called
 * from interrupt context on vxWorks and RTEMS where epicsAtomic
 * operations are ISR-safe (where epicsAtomic has to fall back to a lock,
 * that lock is ISR-safe as well).
 *
 * All writes a thread makes to a node before pushing it are visible to
 * the thread that takes it.
 *
 * epicsLifoTakeAll() returns the nodes newest first. Use
 * epicsLifoReverse() to process them in the order they were pushed;
 * per producer, nodes then appear in push order.
 *
 * \code
 * typedef struct {
 *     epicsLifoNode node;   // conventionally the first member
 *     int value;
 * } item;
 *
 * static epicsLifo queue = EPICS_LIFO_INIT;
 *
 * // producers (any thread, or ISR)
 * if (epicsLifoPush(&queue, &pItem->node))
 *     epicsEventTrigger(wakeConsumer);   // list was empty before
 *
 * // consumer
 * epicsLifoNode *n = epicsLifoReverse(epicsLifoTakeAll(&queue));
 * while (n) {
 *     item *pItem = (item *) n;
 *     n = n->next;    // read before the item may be reused
 *     process(pItem);
 * }
 * \endcode
 *
 * \since UNRELEASED
 */

#ifndef INC_epicsLifo_H
#define INC_epicsLifo_H

#include <stddef.h>

#include "epicsAtomic.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A list node, to be embedded in the user's structure */
typedef struct epicsLifoNode {
    /** \brief Next (older) node, NULL at the end of the list */
    struct epicsLifoNode *next;
} epicsLifoNode;

/** \brief The list head */
typedef struct epicsLifo {
    /** \private Newest node, or NULL. Accessed atomically only. */
    EpicsAtomicPtrT first;
} epicsLifo;

/** \brief Static initializer for an empty epicsLifo */
#define EPICS_LIFO_INIT { NULL }

/** \brief Initialize an epicsLifo to empty
 *
 * Must not be called while other threads access the list.
 */
static EPICS_ALWAYS_INLINE
void epicsLifoInit ( epicsLifo * pLifo )
{
    epicsAtomicSetPtrTRelaxed ( & pLifo->first, NULL );
}

/** \brief Push a chain of nodes
 *
 * Atomically adds the chain \p pFirst ... \p pLast, already linked
 * through their \c next members, to the list. \p pFirst becomes the
 * newest node. The value of \p pLast->next is overwritten.
 *
 * \return non-zero if the list was empty before
 */
static EPICS_ALWAYS_INLINE
int epicsLifoPushBatch ( epicsLifo * pLifo, epicsLifoNode * pFirst,
                         epicsLifoNode * pLast )
{
    EpicsAtomicPtrT head = epicsAtomicGetPtrTRelaxed ( & pLifo->first );
    EpicsAtomicPtrT prev;

    /* The compare-and-swap is a full barrier, so it also publishes the
     * nodes' contents and pLast->next. */
    for ( ;; ) {
        pLast->next = ( epicsLifoNode * ) head;
        prev = epicsAtomicCmpAndSwapPtrT ( & pLifo->first, head, pFirst );
        if ( prev == head )
            break;
        head = prev;
    }
    return head == NULL;
}

/** \brief Push one node
 *
 * \return non-zero if the list was empty before
 */
static EPICS_ALWAYS_INLINE
int epicsLifoPush ( epicsLifo * pLifo, epicsLifoNode * pNode )
{
    return epicsLifoPushBatch ( pLifo, pNode, pNode );
}

/** \brief Remove and return all nodes, newest first
 *
 * \return the newest node, linked through \c next to older ones,
 * or NULL if the list was empty
 */
static EPICS_ALWAYS_INLINE
epicsLifoNode * epicsLifoTakeAll ( epicsLifo * pLifo )
{
    /* The exchange is a full barrier, so the nodes' contents written
     * before their push are visible here. */
    return ( epicsLifoNode * ) epicsAtomicExchangePtrT ( & pLifo->first, NULL );
}

/** \brief Test whether the list is empty
 *
 * The answer may be outdated by the time the caller looks at it, so this
 * is only a hint, e.g. to skip a call of epicsLifoTakeAll().
 */
static EPICS_ALWAYS_INLINE
int epicsLifoIsEmpty ( const epicsLifo * pLifo )
{
    return epicsAtomicGetPtrTRelaxed ( & pLifo->first ) == NULL;
}

/** \brief Reverse a chain of nodes returned by epicsLifoTakeAll()
 *
 * This is a plain (not atomic) list operation on a chain the caller owns.
 *
 * \return the new first node (the oldest one), or NULL
 */
static EPICS_ALWAYS_INLINE
epicsLifoNode * epicsLifoReverse ( epicsLifoNode * pNode )
{
    epicsLifoNode * pReversed = NULL;

    while ( pNode ) {
        epicsLifoNode * const pNext = pNode->next;
        pNode->next = pReversed;
        pReversed = pNode;
        pNode = pNext;
    }
    return pReversed;
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* INC_epicsLifo_H */
