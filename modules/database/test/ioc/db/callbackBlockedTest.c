/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Work queued behind a callback that blocks must run on another worker
 * as soon as one is free. With NWORKERS parallel threads, NWORKERS-1
 * workers are held inside a callback; the last one takes a blocking
 * callback followed by NSHORT short ones and blocks too. Releasing one
 * held worker must then run the short ones while their own worker stays
 * blocked.
 */

#include <stdlib.h>

#include "callback.h"
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define NWORKERS 3
#define NSHORT 20

typedef struct {
    epicsCallback cb;
    epicsEventId started;
    epicsEventId release;
} gate;

static epicsCallback shortCb[NSHORT];
static epicsEventId shortDone;
static int nShortRun;

static void gateCallback(epicsCallback *pcb)
{
    gate *g;

    callbackGetUser(g, pcb);
    epicsEventMustTrigger(g->started);
    epicsEventMustWait(g->release);
}

static void shortCallback(epicsCallback *pcb)
{
    if (epicsAtomicIncrIntT(&nShortRun) == NSHORT)
        epicsEventMustTrigger(shortDone);
}

static void gateInit(gate *g)
{
    g->started = epicsEventMustCreate(epicsEventEmpty);
    g->release = epicsEventMustCreate(epicsEventEmpty);
    callbackSetCallback(gateCallback, &g->cb);
    callbackSetUser(g, &g->cb);
    callbackSetPriority(priorityLow, &g->cb);
}

static void gateDestroy(gate *g)
{
    epicsEventDestroy(g->started);
    epicsEventDestroy(g->release);
}

MAIN(callbackBlockedTest)
{
    gate held[NWORKERS];
    gate hold;
    int i;

    testPlan(2);

    callbackParallelThreads(NWORKERS, "");
    callbackInit();

    shortDone = epicsEventMustCreate(epicsEventEmpty);
    for (i = 0; i < NWORKERS; i++)
        gateInit(&held[i]);
    gateInit(&hold);
    for (i = 0; i < NSHORT; i++) {
        callbackSetCallback(shortCallback, &shortCb[i]);
        callbackSetPriority(priorityLow, &shortCb[i]);
    }

    /* hold every worker inside a callback */
    for (i = 0; i < NWORKERS - 1; i++) {
        callbackRequest(&held[i].cb);
        epicsEventMustWait(held[i].started);
    }
    callbackRequest(&hold.cb);
    epicsEventMustWait(hold.started);

    /* nobody is free: a blocking callback and the short ones queue up */
    callbackRequest(&held[NWORKERS - 1].cb);
    for (i = 0; i < NSHORT; i++)
        callbackRequest(&shortCb[i]);

    /* the released worker takes the blocking callback and blocks in it */
    epicsEventMustTrigger(hold.release);
    epicsEventMustWait(held[NWORKERS - 1].started);
    testDiag("%d of %d short callbacks ran before a worker was freed",
        epicsAtomicGetIntT(&nShortRun), NSHORT);

    /* free one worker: it must run the short ones */
    epicsEventMustTrigger(held[0].release);
    testOk(epicsEventWaitWithTimeout(shortDone, 5.0) == epicsEventOK,
        "%d short callbacks ran while their worker stayed blocked", NSHORT);
    testOk(epicsAtomicGetIntT(&nShortRun) == NSHORT,
        "%d short callbacks ran", epicsAtomicGetIntT(&nShortRun));

    for (i = 1; i < NWORKERS; i++)
        epicsEventMustTrigger(held[i].release);

    callbackStop();
    callbackCleanup();

    for (i = 0; i < NWORKERS; i++)
        gateDestroy(&held[i]);
    gateDestroy(&hold);
    epicsEventDestroy(shortDone);

    return testDone();
}
