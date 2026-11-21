/* osi/os/Linux/osdEvent.c
 *
 * epicsEvent on a futex: no mutex, so no owner to inherit from and no
 * priority-inheritance hand-off between epicsEventTrigger() and a woken
 * epicsEventWait(). state is 0 (empty), 2 (full, wake owed) or 1 (full,
 * woken); waiters counts threads that may be blocked in futex_wait, so
 * a trigger with no waiter makes no system call, and neither does one
 * that finds the event full and woken.
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

struct epicsEventOSD {
    int state;      /* 0 empty, 2 full and wake owed, 1 full and woken */
    int waiters;    /* threads in or about to enter futex_wait */
};

/* the layout SYS_futex reads on this ABI: {long, long} is
 * __kernel_timespec on LP64 and old_timespec32 on 32-bit. The libc
 * struct timespec differs where time_t is 64-bit on a 32-bit target
 * (musl, glibc with _TIME_BITS=64). The deadline is CLOCK_MONOTONIC,
 * so tv_sec is uptime and fits a 32-bit long. */
struct futexTimespec { long tv_sec; long tv_nsec; };

static long futex(int *uaddr, int op, int val, const struct futexTimespec *to)
{
    return syscall(SYS_futex, uaddr, op, val, to, NULL, FUTEX_BITSET_MATCH_ANY);
}

LIBCOM_API epicsEventId epicsEventCreate(epicsEventInitialState init)
{
    epicsEventId pevent = calloc(1, sizeof(*pevent));

    if (pevent)
        pevent->state = (init == epicsEventFull);
    return pevent;
}

LIBCOM_API void epicsEventDestroy(epicsEventId pevent)
{
    free(pevent);
}

LIBCOM_API epicsEventStatus epicsEventTrigger(epicsEventId pevent)
{
    /* one wake per fill: a waiter blocks only on an empty event, so a
     * full and woken one has nobody left to wake and a trigger that
     * finds it costs no system call. The wake stays owed until it is
     * made, so a thread stopped between its fill and its futex_wake
     * leaves the wake to the next trigger and the waiter does not
     * depend on that thread running again. */
    for (;;) {
        int s = epicsAtomicGetIntT(&pevent->state);
        if (s == 1)
            return epicsEventOK;
        if (epicsAtomicCmpAndSwapIntT(&pevent->state, s, 2) == s)
            break;
    }
    if (epicsAtomicGetIntT(&pevent->waiters) > 0)
        futex(&pevent->state, FUTEX_WAKE_PRIVATE, 1, NULL);
    epicsAtomicCmpAndSwapIntT(&pevent->state, 2, 1);
    return epicsEventOK;
}

/* consume the event if full */
static int take(epicsEventId pevent)
{
    for (;;) {
        int s = epicsAtomicGetIntT(&pevent->state);
        if (s == 0)
            return 0;
        if (epicsAtomicCmpAndSwapIntT(&pevent->state, s, 0) == s)
            return 1;
    }
}

/* deadline NULL: wait forever */
static epicsEventStatus waitUntil(epicsEventId pevent, const struct futexTimespec *deadline)
{
    epicsEventStatus result = epicsEventOK;

    if (take(pevent))
        return epicsEventOK;
    epicsAtomicIncrIntT(&pevent->waiters);
    while (!take(pevent)) {
        if (futex(&pevent->state, FUTEX_WAIT_BITSET_PRIVATE, 0, deadline) < 0) {
            if (errno == ETIMEDOUT) {
                result = take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
                break;
            }
            if (errno != EAGAIN && errno != EINTR) {
                errlogPrintf("epicsEventWait: futex_wait failed: %s\n", strerror(errno));
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
    struct futexTimespec deadline;

    if (timeout <= 0.0)
        return take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (timeout > 3600.0 * 24 * 365) timeout = 3600.0 * 24 * 365;
    deadline.tv_sec = (long)now.tv_sec + (long)timeout;
    deadline.tv_nsec = now.tv_nsec + (long)((timeout - (long)timeout) * 1e9);
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
    printf("epicsEvent %p: %s, %d waiters\n", pevent,
        pevent->state ? "full" : "empty", pevent->waiters);
}
