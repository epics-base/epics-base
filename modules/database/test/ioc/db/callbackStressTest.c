/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* NREQ requester threads each queue their NCB callbacks, spread over
 * the three priorities, NREQUESTS times against NWORKERS workers per
 * priority. Some callbacks sleep inside, so their worker is held and
 * the rest must move to others; some queue themselves again, so a
 * worker is a requester too. Every accepted request must run exactly
 * once, and once the requesters are done every worker must be asleep
 * with nothing in use. Each round starts from that idle state, so a
 * round also checks that the first request wakes a fully asleep queue.
 */

#include <stdlib.h>

#include "callback.h"
#include "dbAccessDefs.h"
#include "dbUnitTest.h"
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define NWORKERS 4
#define NREQ 4
#define NCB 6
#define NREQUESTS 4000
#define NCHAIN 50
#define NROUNDS 3

typedef struct {
    epicsCallback cb;
    int ran;        /* atomic */
    int accepted;   /* atomic */
    int chain;      /* atomic: queue myself again while positive */
} slot;

typedef struct {
    slot s[NCB];
    unsigned seed;
    epicsThreadId tid;
} requester;

static requester reqs[NREQ];

static int request(slot *s)
{
    int status = callbackRequest(&s->cb);

    if (status == 0)
        epicsAtomicIncrIntT(&s->accepted);
    return status;
}

static void slotCallback(epicsCallback *pcb)
{
    slot *s;

    callbackGetUser(s, pcb);
    if ((epicsAtomicIncrIntT(&s->ran) & 127) == 0)
        epicsThreadSleep(0.001);
    if (epicsAtomicDecrIntT(&s->chain) >= 0)
        request(s);
}

static void requesterThread(void *arg)
{
    requester *r = arg;
    int i;

    for (i = 0; i < NREQUESTS; i++) {
        r->seed = r->seed * 1103515245u + 12345u;
        /* a full queue is not the point here: back off and retry */
        while (request(&r->s[(r->seed >> 16) % NCB]) == S_db_bufFull)
            epicsThreadSleep(0.001);
    }
}

static int waitIdle(void)
{
    int i;

    for (i = 0; i < 2000; i++) {
        if (testCallbackIdle()) return 1;
        epicsThreadSleep(0.005);
    }
    return 0;
}

MAIN(callbackStressTest)
{
    callbackQueueStats st;
    int round, i, j;

    testPlan(NROUNDS * 3);

    callbackParallelThreads(NWORKERS, "");
    callbackInit();

    for (i = 0; i < NREQ; i++) {
        reqs[i].seed = i + 1;
        for (j = 0; j < NCB; j++) {
            slot *s = &reqs[i].s[j];
            callbackSetCallback(slotCallback, &s->cb);
            callbackSetUser(s, &s->cb);
            callbackSetPriority(j % NUM_CALLBACK_PRIORITIES, &s->cb);
        }
    }

    for (round = 1; round <= NROUNDS; round++) {
        int mismatched = 0, accepted = 0, idle;

        for (i = 0; i < NREQ; i++)
            for (j = 0; j < NCB; j++) {
                slot *s = &reqs[i].s[j];
                epicsAtomicSetIntT(&s->ran, 0);
                epicsAtomicSetIntT(&s->accepted, 0);
                epicsAtomicSetIntT(&s->chain, NCHAIN);
            }
        for (i = 0; i < NREQ; i++) {
            epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
            opts.joinable = 1;
            reqs[i].tid = epicsThreadCreateOpt("requester", requesterThread,
                &reqs[i], &opts);
        }
        for (i = 0; i < NREQ; i++)
            epicsThreadMustJoin(reqs[i].tid);

        idle = waitIdle();
        if (idle) {
            epicsThreadSleep(0.05);
            idle = testCallbackIdle();
        }
        testOk(idle, "round %d: all workers asleep and stay so", round);

        for (i = 0; i < NREQ; i++)
            for (j = 0; j < NCB; j++) {
                slot *s = &reqs[i].s[j];
                int a = epicsAtomicGetIntT(&s->accepted);
                int r = epicsAtomicGetIntT(&s->ran);
                if (a != r) {
                    testDiag("requester %d slot %d: %d accepted, %d ran", i, j, a, r);
                    mismatched++;
                }
                accepted += a;
            }
        testOk(mismatched == 0, "round %d: %d accepted requests ran exactly once",
            round, accepted);

        callbackQueueStatus(1, &st);
        testOk(st.numUsed[priorityLow] == 0 && st.numUsed[priorityMedium] == 0 &&
            st.numUsed[priorityHigh] == 0,
            "round %d: nothing in use (%d %d %d), overflows %d %d %d", round,
            st.numUsed[priorityLow], st.numUsed[priorityMedium], st.numUsed[priorityHigh],
            st.numOverflow[priorityLow], st.numOverflow[priorityMedium],
            st.numOverflow[priorityHigh]);
    }

    callbackStop();
    callbackCleanup();

    return testDone();
}
