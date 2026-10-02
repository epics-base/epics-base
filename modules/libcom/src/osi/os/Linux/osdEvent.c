/* osi/os/Linux/osdEvent.c
 *
 * epicsEvent on a futex: no mutex, so no owner to inherit from and no
 * priority-inheritance hand-off between epicsEventTrigger() and a woken
 * epicsEventWait(). state is 0 (empty) or 1 (full); waiters counts
 * threads that may be blocked in futex_wait, so a trigger with no waiter
 * is a single atomic exchange.
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

#include "epicsEvent.h"
#include "errlog.h"

struct epicsEventOSD {
    int state;      /* 0 empty, 1 full */
    int waiters;    /* threads in or about to enter futex_wait */
};

static long futex(int *uaddr, int op, int val, const struct timespec *to)
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
    /* wake whenever a waiter is registered, even if the event was
     * already full: the thread that filled it may be stopped between
     * its exchange and its futex_wake, and the waiter must not depend
     * on that thread running again */
    __atomic_exchange_n(&pevent->state, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&pevent->waiters, __ATOMIC_SEQ_CST) > 0)
        futex(&pevent->state, FUTEX_WAKE_PRIVATE, 1, NULL);
    return epicsEventOK;
}

/* consume the event if full */
static int take(epicsEventId pevent)
{
    int full = 1;
    return __atomic_compare_exchange_n(&pevent->state, &full, 0, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* deadline NULL: wait forever */
static epicsEventStatus waitUntil(epicsEventId pevent, const struct timespec *deadline)
{
    epicsEventStatus result = epicsEventOK;

    if (take(pevent))
        return epicsEventOK;
    __atomic_add_fetch(&pevent->waiters, 1, __ATOMIC_SEQ_CST);
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
    __atomic_sub_fetch(&pevent->waiters, 1, __ATOMIC_SEQ_CST);
    return result;
}

LIBCOM_API epicsEventStatus epicsEventWait(epicsEventId pevent)
{
    return waitUntil(pevent, NULL);
}

LIBCOM_API epicsEventStatus epicsEventWaitWithTimeout(epicsEventId pevent,
    double timeout)
{
    struct timespec deadline;

    if (timeout <= 0.0)
        return take(pevent) ? epicsEventOK : epicsEventWaitTimeout;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    if (timeout > 3600.0 * 24 * 365) timeout = 3600.0 * 24 * 365;
    deadline.tv_sec += (time_t)timeout;
    deadline.tv_nsec += (long)((timeout - (time_t)timeout) * 1e9);
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
