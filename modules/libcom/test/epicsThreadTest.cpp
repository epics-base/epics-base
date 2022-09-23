/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsThreadTest.cpp */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

namespace {

static epicsThreadPrivate<int> privateKey;

class myThread: public epicsThreadRunable {
public:
    myThread(int arg,const char *name);
    virtual ~myThread();
    virtual void run();
    epicsThread thread;
    epicsEvent startEvt;
private:
    int *argvalue;
};

myThread::myThread(int arg,const char *name) :
    thread(*this,name,epicsThreadStackSmall,50+arg),
    argvalue(0)
{
    argvalue = new int;
    *argvalue = arg;
    thread.start();
}

myThread::~myThread() {delete argvalue;}

void myThread::run()
{
    startEvt.signal();
    int *pset = argvalue;
    privateKey.set(argvalue);

    int *pget = privateKey.get();
    testOk1(pget == pset);

    epicsThreadId self = epicsThreadGetIdSelf();
    testOk1(thread.getPriority() == epicsThreadGetPriority(self));
}

void testMyThread()
{

    const int ntasks = 3;
    myThread *myThreads[ntasks];

    int startPriority = 0;
    for (int i = 0; i < ntasks; i++) {
        char name[10];
        sprintf(name, "t%d", i);
        myThreads[i] = new myThread(i, name);
        if (i == 0)
            startPriority = myThreads[i]->thread.getPriority();
        myThreads[i]->thread.setPriority(startPriority + i);
    }

    for (int i = 0; i < ntasks; i++) {
        myThreads[i]->startEvt.wait();
        myThreads[i]->thread.exitWait();
        delete myThreads[i];
    }
}

struct joinStuff {
    epicsThreadOpts *opts;
    epicsEvent *trigger;
    epicsEvent *finished;
};

void donothing(void *arg)
{}

void dowait(void *arg)
{
    epicsEvent *trigger = (epicsEvent *) arg;
    trigger->wait();
    epicsThreadSleep(0.1);
}

void dodelay(void *arg)
{
    epicsThreadSleep(2.0);
}

epicsThreadId joiningThread;

void checkJoined(epicsThreadId thread)
{
    char name[80] = "?";

    if(thread==joiningThread) {
        epicsThreadGetName(thread, name, sizeof(name));
        testFail("Joined thread '%s' is still alive!", name);
    }
}

void joinTests(void *arg)
{
    struct joinStuff *stuff = (struct joinStuff *) arg;

    // Task finishes before parent joins
    epicsThreadId tid = epicsThreadCreateOpt("nothing",
        &donothing, 0, stuff->opts);
    epicsThreadSleep(0.1);
    epicsThreadMustJoin(tid);

    joiningThread = tid;
    epicsThreadMap(checkJoined);

    // Parent joins before task finishes
    tid = epicsThreadCreateOpt("await",
        &dowait, stuff->trigger, stuff->opts);
    stuff->trigger->signal();
    epicsThreadMustJoin(tid);

    joiningThread = tid;
    epicsThreadMap(checkJoined);

    // Parent gets delayed until task finishes
    epicsTime start, end;
    start = epicsTime::getCurrent();
    tid = epicsThreadCreateOpt("delay",
        &dodelay, 0, stuff->opts);
    epicsThreadMustJoin(tid);
    end = epicsTime::getCurrent();
    double duration = end - start;
#ifndef EPICS_THREAD_CAN_JOIN
    testTodoBegin("Thread join doesn't work");
#endif
    testOk(duration > 1.0, "Join delayed parent (%g seconds)", duration);
    testTodoEnd();

    joiningThread = tid;
    epicsThreadMap(checkJoined);

    // This is a no-op
    epicsThreadId self = epicsThreadGetIdSelf();
    epicsThreadMustJoin(self);
    tid = epicsThreadGetIdSelf();
    testOk(self==tid, "%p == %p avoid self re-alloc", self, tid);

    // This is a no-op as well, except for a warning.
    eltc(0);
    epicsThreadMustJoin(self);
    eltc(1);

    stuff->finished->signal();
}

void testJoining()
{
    epicsThreadOpts opts1 = EPICS_THREAD_OPTS_INIT;
    epicsThreadOpts opts2 = EPICS_THREAD_OPTS_INIT;
    epicsEvent finished, trigger;
    struct joinStuff stuff = {
        &opts1, &trigger, &finished
    };

    opts1.priority = 50;
    opts2.priority = 40;
    opts1.joinable = 1;
    opts2.joinable = 1;
    epicsThreadCreateOpt("parent", &joinTests, &stuff, &opts2);

    // Thread 'parent' joins itself, so we can't.
    testOk(finished.wait(10.0), "Join tests #1 completed");

    // Repeat with opposite thread priorities
    stuff.opts = &opts2;
    epicsThreadCreateOpt("parent", &joinTests, &stuff, &opts1);
    testOk(finished.wait(10.0), "Join tests #2 completed");
}

} // namespace

typedef struct info {
    int  isOkToBlock;
    int  didSomething;
} info;

extern "C" {
static void thread(void *arg)
{
    info *pinfo = (info *)arg;

    epicsThreadSetOkToBlock(pinfo->isOkToBlock);

    testOk(epicsThreadIsOkToBlock() == pinfo->isOkToBlock,
        "%s epicsThreadIsOkToBlock() = %d",
        epicsThreadGetNameSelf(), pinfo->isOkToBlock);

    pinfo->didSomething = 1;
}
}

static void testOkToBlock()
{
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;

    opts.priority = 50;
    opts.joinable = 1;

    info infoA = {0, 0};
    epicsThreadId threadA = epicsThreadCreateOpt("threadA", thread, &infoA, &opts);

    info infoB = {1, 0};
    epicsThreadId threadB = epicsThreadCreateOpt("threadB", thread, &infoB, &opts);

    // join B first to better our chance of detecting if it never runs.
    epicsThreadMustJoin(threadB);
    testOk1(infoB.didSomething);

    epicsThreadMustJoin(threadA);
    testOk1(infoA.didSomething);
}


MAIN(epicsThreadTest)
{
    testPlan(17);

    unsigned int ncpus = epicsThreadGetCPUs();
    testDiag("System has %u CPUs", ncpus);
    testOk1(ncpus > 0);
    testDiag("main() thread %p", epicsThreadGetIdSelf());

    testJoining(); // Do this first, ~epicsThread() uses it...
    testMyThread();
    testOkToBlock();

    // attempt to self-join from a non-EPICS thread
    // to make sure it does nothing as expected
    eltc(0);
    epicsThreadMustJoin(epicsThreadGetIdSelf());
    eltc(1);

    return testDone();
}
