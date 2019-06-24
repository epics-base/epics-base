/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
    thread(*this,name,epicsThreadGetStackSize(epicsThreadStackSmall),50+arg),
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

struct selfJoiner {
    epicsEvent finished;
};

void joiner(void *arg) {
    epicsEvent *finished = (epicsEvent*)arg;

    // This is a no-op
    epicsThreadMustJoin(epicsThreadGetIdSelf());

    // This is a no-op as well, except for a warning.
    eltc(0);
    epicsThreadMustJoin(epicsThreadGetIdSelf());
    eltc(1);

    testPass("Check double self-join");
    finished->signal();
}

typedef struct info {
    int  isOkToBlock;
    int  didSomething;
} info;

void testSelfJoin()
{
    epicsEvent finished;
    epicsThreadOpts opts;
    epicsThreadOptsDefaults(&opts);
    opts.priority = 50;
    opts.joinable = 1;

    (void)epicsThreadCreateOpt("selfjoin", &joiner, &finished, &opts);

    // as this thread "joins" itself, we can't.
    finished.wait();
}

} // namespace

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

    epicsThreadOpts opts;
    epicsThreadOptsDefaults(&opts);
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
    testPlan(12);

    unsigned int ncpus = epicsThreadGetCPUs();
    testDiag("System has %u CPUs", ncpus);
    testOk1(ncpus > 0);
    testDiag("main() thread %p", epicsThreadGetIdSelf());

    testMyThread();
    testSelfJoin();
    testOkToBlock();

    // attempt to self-join from a non-EPICS thread
    // to make sure it does nothing as expected
    eltc(0);
    epicsThreadMustJoin(epicsThreadGetIdSelf());
    eltc(1);

    return testDone();
}
