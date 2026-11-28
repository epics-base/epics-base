/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* epicsEventPerform.cpp
 *
 * Performance measurements for epicsEvent. Not run by "make runtests".
 *
 * A ping-pong: two threads (prio 80) hand control back and forth
 *   through two events; round-trip latency.
 * B inversion: a high-priority thread (prio 90) triggers an event every
 *   500 us and measures how long epicsEventTrigger() takes, while
 *   low-priority threads (prio 10) hammer the same event with trigger and
 *   try-wait, and duty-cycled mid-priority hogs (prio 50) preempt them at
 *   random points. A high-priority waiter (prio 91) measures the wake-up
 *   latency through a second, uncontended event.
 * C throughput: 4 triggering and 4 waiting threads (prio 50).
 *
 * Run it both without and with real-time scheduling: on Linux, as a user
 * with a non-zero "ulimit -r" or as root (e.g. via sudo). Keep the total
 * real-time load in B below the kernel's RT throttling limit
 * (/proc/sys/kernel/sched_rt_runtime_us), otherwise all real-time threads
 * get stalled for milliseconds and the results are meaningless.
 *
 * Environment variables:
 *   EVPERF_TESTS     scenarios to run, default "ABC"
 *   EVPERF_LOWS      number of low-priority threads in B, default ncpu/3
 *   EVPERF_HOGS      number of hog threads in B, default ncpu/2
 *   EVPERF_TRIGGERS  triggers per thread in C, default 100000
 */

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

#ifdef __linux__
#  include <pthread.h>
#  include <sched.h>
#endif

#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "epicsAtomic.h"
#include "epicsUnitTest.h"
#include "testMain.h"

namespace {

double now()
{
    return epicsMonotonicGet() * 1e-9;
}

int envInt(const char *name, int dflt)
{
    const char *val = getenv(name);
    int ret = val ? atoi(val) : dflt;
    return ret > 0 ? ret : dflt;
}

void report(const char *what, std::vector<double> &v)
{
    if (v.empty()) {
        testDiag("  %-26s no samples", what);
        return;
    }
    std::sort(v.begin(), v.end());
    double sum = 0.0;
    for (size_t i = 0; i < v.size(); i++)
        sum += v[i];
    testDiag("  %-26s n=%-6lu mean %7.2f  p50 %7.2f  p99 %8.2f"
        "  p99.9 %8.2f  max %9.2f us",
        what, (unsigned long) v.size(), sum / v.size() * 1e6,
        v[v.size() / 2] * 1e6, v[(size_t) (v.size() * 0.99)] * 1e6,
        v[(size_t) (v.size() * 0.999)] * 1e6, v.back() * 1e6);
}

epicsThreadId start(const char *name, unsigned prio, EPICSTHREADFUNC func,
    void *arg)
{
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;
    opts.priority = prio;
    opts.stackSize = epicsThreadStackSmall;
    epicsThreadId tid = epicsThreadCreateOpt(name, func, arg, &opts);
    if (!tid)
        testAbort("Failed to create thread %s", name);
    return tid;
}

/* ---- A: ping-pong ---- */

const int ppRounds = 100000;
epicsEventId ping, pong;
const char *schedPolicy = "unknown";

void ponger(void *)
{
#ifdef __linux__
    int policy;
    struct sched_param param;
    if (pthread_getschedparam(pthread_self(), &policy, &param) == 0)
        schedPolicy = policy == SCHED_FIFO ? "SCHED_FIFO" :
            policy == SCHED_RR ? "SCHED_RR" : "SCHED_OTHER";
#endif
    for (int i = 0; i < ppRounds; i++) {
        epicsEventMustWait(ping);
        epicsEventMustTrigger(pong);
    }
}

struct pingerArg {
    std::vector<double> rtt;
};

void pinger(void *raw)
{
    pingerArg *pArg = static_cast<pingerArg *>(raw);
    pArg->rtt.reserve(ppRounds);
    for (int i = 0; i < ppRounds; i++) {
        double t0 = now();
        epicsEventMustTrigger(ping);
        epicsEventMustWait(pong);
        pArg->rtt.push_back(now() - t0);
    }
}

void testPingPong()
{
    pingerArg arg;

    ping = epicsEventMustCreate(epicsEventEmpty);
    pong = epicsEventMustCreate(epicsEventEmpty);
    epicsThreadId t1 = start("ponger", 80, ponger, 0);
    epicsThreadId t2 = start("pinger", 80, pinger, &arg);
    epicsThreadMustJoin(t2);
    epicsThreadMustJoin(t1);

    testDiag("A ping-pong (prio 80/80), scheduling policy %s", schedPolicy);
    report("round trip", arg.rtt);

    epicsEventDestroy(ping);
    epicsEventDestroy(pong);
}

/* ---- B: priority inversion ---- */

epicsEventId evContended, evWake;
int stopB;
double tTrigger;            /* time of the last high-priority trigger */
int wakePending;

void lowUser(void *)
{
    unsigned n = 0;
    while (!epics::atomic::get(stopB)) {
        epicsEventTrigger(evContended);
        epicsEventTryWait(evContended);
        if (++n % 4096u == 0u)
            epicsThreadSleep(0.0);
    }
}

void hog(void *)
{
    while (!epics::atomic::get(stopB)) {
        double busyUntil = now() + 0.002;   /* busy 2 ms */
        while (now() < busyUntil) {}
        epicsThreadSleep(0.001);            /* then idle about 1 ms */
    }
}

struct waiterArg {
    std::vector<double> latency;
};

void hiWaiter(void *raw)
{
    waiterArg *pArg = static_cast<waiterArg *>(raw);
    while (!epics::atomic::get(stopB)) {
        if (epicsEventWaitWithTimeout(evWake, 0.1) != epicsEventOK)
            continue;
        double t1 = now();
        if (epics::atomic::compareAndSwap(wakePending, 1, 0) == 1)
            pArg->latency.push_back(t1 - tTrigger);
    }
}

struct triggerArg {
    std::vector<double> duration;
};

void hiTrigger(void *raw)
{
    triggerArg *pArg = static_cast<triggerArg *>(raw);
    double end = now() + 5.0;
    while (now() < end) {
        double t0 = now();
        epicsEventTrigger(evContended);
        pArg->duration.push_back(now() - t0);

        tTrigger = now();
        epics::atomic::set(wakePending, 1);
        epicsEventTrigger(evWake);
        epicsThreadSleep(0.0005);
    }
}

void testInversion()
{
    int ncpu = epicsThreadGetCPUs();
    int nLow = envInt("EVPERF_LOWS", ncpu / 3 > 0 ? ncpu / 3 : 1);
    int nHog = envInt("EVPERF_HOGS", ncpu / 2 > 0 ? ncpu / 2 : 1);
    std::vector<epicsThreadId> tids;
    waiterArg wArg;
    triggerArg tArg;

    evContended = epicsEventMustCreate(epicsEventEmpty);
    evWake = epicsEventMustCreate(epicsEventEmpty);
    stopB = 0;
    for (int i = 0; i < nLow; i++)
        tids.push_back(start("low", 10, lowUser, 0));
    for (int i = 0; i < nHog; i++)
        tids.push_back(start("hog", 50, hog, 0));
    epicsThreadId tw = start("hiWait", 91, hiWaiter, &wArg);
    epicsThreadSleep(0.2);
    epicsThreadId tt = start("hiTrig", 90, hiTrigger, &tArg);
    epicsThreadMustJoin(tt);
    epics::atomic::set(stopB, 1);
    epicsEventTrigger(evWake);
    epicsThreadMustJoin(tw);
    for (size_t i = 0; i < tids.size(); i++)
        epicsThreadMustJoin(tids[i]);

    testDiag("B inversion (%d low-prio users, %d hogs, %d CPUs, 5 s)",
        nLow, nHog, ncpu);
    report("hi-prio trigger duration", tArg.duration);
    report("hi-prio wake-up latency", wArg.latency);

    epicsEventDestroy(evContended);
    epicsEventDestroy(evWake);
}

/* ---- C: throughput ---- */

epicsEventId evThroughput;
int stopC, taken, triggersPerThread;

void cWaiter(void *)
{
    while (!epics::atomic::get(stopC)) {
        if (epicsEventWaitWithTimeout(evThroughput, 0.01) == epicsEventOK)
            epics::atomic::increment(taken);
    }
}

void cTrigger(void *)
{
    for (int i = 0; i < triggersPerThread; i++)
        epicsEventTrigger(evThroughput);
}

void testThroughput()
{
    const int nThreads = 4;
    epicsThreadId waiters[nThreads], triggerers[nThreads];

    triggersPerThread = envInt("EVPERF_TRIGGERS", 100000);
    evThroughput = epicsEventMustCreate(epicsEventEmpty);
    stopC = taken = 0;
    for (int i = 0; i < nThreads; i++)
        waiters[i] = start("cWait", 50, cWaiter, 0);
    epicsThreadSleep(0.1);
    double t0 = now();
    for (int i = 0; i < nThreads; i++)
        triggerers[i] = start("cTrig", 50, cTrigger, 0);
    for (int i = 0; i < nThreads; i++)
        epicsThreadMustJoin(triggerers[i]);
    double elapsed = now() - t0;
    epics::atomic::set(stopC, 1);
    for (int i = 0; i < nThreads; i++)
        epicsThreadMustJoin(waiters[i]);

    testDiag("C throughput (%d triggering, %d waiting threads, prio 50)",
        nThreads, nThreads);
    testDiag("  %d triggers in %.3f s (%.0f ns each), %d taken",
        nThreads * triggersPerThread, elapsed,
        elapsed * 1e9 / (nThreads * triggersPerThread), taken);

    epicsEventDestroy(evThroughput);
}

} // namespace

MAIN(epicsEventPerform)
{
    const char *tests = getenv("EVPERF_TESTS");

    if (!tests)
        tests = "ABC";
    testPlan(0);
    if (strchr(tests, 'A'))
        testPingPong();
    if (strchr(tests, 'B'))
        testInversion();
    if (strchr(tests, 'C'))
        testThroughput();
    return testDone();
}
