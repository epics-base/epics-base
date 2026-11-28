/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* osi/os/Linux/osdEvent.c
 *
 * epicsEvent on a Linux futex.
 *
 * Unlike the POSIX implementation (a mutex and a condition variable),
 * neither epicsEventTrigger() nor a waiter that is being woken has to
 * take a lock that another thread may hold, so a high-priority thread
 * never waits for a preempted low-priority one inside these functions,
 * whether or not priority inheritance is available. A trigger that finds
 * no waiter makes no system call. The kernel wakes the highest-priority
 * waiter first, and timeouts use CLOCK_MONOTONIC.
 *
 * The futex word "state" holds
 *   EV_FULL   the event is full
 *   EV_OWED   (only with EV_FULL) a wake-up for this fill may still be
 *             outstanding
 *   seq       the remaining bits, incremented with every fill, so that a
 *             futex wait for an empty state can't miss a fill-and-take,
 *             and a trigger can tell its own fill from a later one
 *
 * A trigger that fills the event sets EV_FULL|EV_OWED, wakes one waiter
 * if there may be any, then clears EV_OWED if the state still holds its
 * fill. A trigger that finds the event full with EV_OWED set wakes
 * a waiter as well, so a waiter never depends on a triggering thread that
 * was preempted between its fill and its wake-up. A trigger that finds
 * the event full without EV_OWED has nothing to do.
 *
 * "waiters" counts the threads in or about to enter a futex wait. Waiters
 * increment it before checking the state, triggers check it after
 * filling; both use full barriers, so either the trigger sees the waiter
 * or the waiter sees the fill.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "errlog.h"

#define EV_FULL 1
#define EV_OWED 2
#define EV_SEQ  4

struct epicsEventOSD {
    int state;
    int waiters;
};

/* The futex system call reads a kernel timespec, which isn't always the
 * libc struct timespec (e.g. on 32-bit targets with a 64-bit time_t).
 */
#if defined(SYS_futex)
#  define EV_SYS_FUTEX SYS_futex
#  if defined(__x86_64__) && defined(__ILP32__) /* x32 */
typedef long long evTime;
#  else
typedef long evTime;
#  endif
#elif defined(SYS_futex_time64)
#  define EV_SYS_FUTEX SYS_futex_time64
typedef long long evTime;
#else
#  error No futex system call available
#endif

typedef struct { evTime tv_sec; evTime tv_nsec; } evTimespec;

static int futexWait(int *pState, int expected, const evTimespec *pDeadline)
{
    /* FUTEX_WAIT_BITSET takes an absolute CLOCK_MONOTONIC deadline */
    return (int) syscall(EV_SYS_FUTEX, pState, FUTEX_WAIT_BITSET_PRIVATE,
        expected, pDeadline, NULL, FUTEX_BITSET_MATCH_ANY);
}

static void futexWakeOne(int *pState)
{
    syscall(EV_SYS_FUTEX, pState, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

LIBCOM_API epicsEventId epicsEventCreate(epicsEventInitialState init)
{
    epicsEventId pevent = calloc(1, sizeof(*pevent));

    if (pevent)
        pevent->state = (init == epicsEventFull) ? EV_FULL : 0;
    return pevent;
}

LIBCOM_API void epicsEventDestroy(epicsEventId pevent)
{
    free(pevent);
}

LIBCOM_API epicsEventStatus epicsEventTrigger(epicsEventId pevent)
{
    int s = epicsAtomicGetIntT(&pevent->state);
    int filled;

    for (;;) {
        int prev;

        if (s & EV_FULL) {
            if (!(s & EV_OWED))
                return epicsEventOK;
            filled = s;     /* help the filling thread */
            break;
        }
        filled = (int) ((unsigned) s + EV_SEQ) | EV_FULL | EV_OWED;
        prev = epicsAtomicCmpAndSwapIntT(&pevent->state, s, filled);
        if (prev == s)
            break;
        s = prev;
    }
    if (epicsAtomicGetIntT(&pevent->waiters) > 0)
        futexWakeOne(&pevent->state);
    epicsAtomicCmpAndSwapIntT(&pevent->state, filled, filled & ~EV_OWED);
    return epicsEventOK;
}

/* Empty the event if it is full, return whether it was */
static int take(epicsEventId pevent)
{
    int s = epicsAtomicGetIntT(&pevent->state);

    while (s & EV_FULL) {
        int prev = epicsAtomicCmpAndSwapIntT(&pevent->state, s,
            s & ~(EV_FULL | EV_OWED));
        if (prev == s)
            return 1;
        s = prev;
    }
    return 0;
}

/* pDeadline NULL: wait forever */
static epicsEventStatus waitUntil(epicsEventId pevent,
    const evTimespec *pDeadline)
{
    epicsEventStatus result;

    if (take(pevent))
        return epicsEventOK;

    epicsAtomicIncrIntT(&pevent->waiters);
    for (;;) {
        int s = epicsAtomicGetIntT(&pevent->state);

        if (s & EV_FULL) {
            if (epicsAtomicCmpAndSwapIntT(&pevent->state, s,
                    s & ~(EV_FULL | EV_OWED)) == s) {
                result = epicsEventOK;
                break;
            }
            continue;
        }
        if (futexWait(&pevent->state, s, pDeadline) != 0) {
            int err = errno;

            if (err == ETIMEDOUT) {
                result = take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
                break;
            }
            if (err != EAGAIN && err != EINTR) {
                errlogPrintf("epicsEventWait: futex wait failed: %s\n",
                    strerror(err));
                result = epicsEventError;
                break;
            }
        }
    }
    epicsAtomicDecrIntT(&pevent->waiters);
    return result;
}

LIBCOM_API epicsEventStatus epicsEventWait(epicsEventId pevent)
{
    return waitUntil(pevent, NULL);
}

LIBCOM_API epicsEventStatus epicsEventWaitWithTimeout(epicsEventId pevent,
    double timeout)
{
    struct timespec now;
    evTimespec deadline;
    evTime sec;

    if (!(timeout > 0.0))   /* also catches NaN */
        return take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
    if (timeout > 60 * 60 * 24 * 3652.5)
        timeout = 60 * 60 * 24 * 3652.5;    /* 10 years */

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        errlogPrintf("epicsEventWaitWithTimeout: clock_gettime failed: %s\n",
            strerror(errno));
        return epicsEventError;
    }
    sec = (evTime) timeout;
    deadline.tv_sec = (evTime) now.tv_sec + sec;
    deadline.tv_nsec = (evTime) now.tv_nsec +
        (evTime) ((timeout - (double) sec) * 1e9);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        deadline.tv_sec++;
    }
    return waitUntil(pevent, &deadline);
}

LIBCOM_API epicsEventStatus epicsEventTryWait(epicsEventId pevent)
{
    return take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
}

LIBCOM_API void epicsEventShow(epicsEventId pevent, unsigned int level)
{
    int s = epicsAtomicGetIntT(&pevent->state);

    printf("epicsEvent %p: %s\n", (void *) pevent,
        (s & EV_FULL) ? "full" : "empty");
    if (level > 0)
        printf("    futex state = 0x%x, waiters = %d\n", (unsigned) s,
            epicsAtomicGetIntT(&pevent->waiters));
}
