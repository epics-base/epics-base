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
#include "epicsTime.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static epicsThreadPrivate<int> privateKey;

class myThread: public epicsThreadRunable {
public:
    myThread(int arg,const char *name);
    virtual ~myThread();
    virtual void run();
    epicsThread thread;
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
    int *pset = argvalue;
    privateKey.set(argvalue);
    epicsThreadSleep(2.0);
    int *pget = privateKey.get();
    testOk1(pget == pset);

    epicsThreadId self = epicsThreadGetIdSelf();
    testOk1(thread.getPriority() == epicsThreadGetPriority(self));
}


typedef struct info {
    int  isOkToBlock;
} info;

extern "C" {
static void thread(void *arg)
{
    info *pinfo = (info *)arg;

    epicsThreadSetOkToBlock(pinfo->isOkToBlock);
    epicsThreadSleep(1.0);

    testOk(epicsThreadIsOkToBlock() == pinfo->isOkToBlock,
        "%s epicsThreadIsOkToBlock() = %d",
        epicsThreadGetNameSelf(), pinfo->isOkToBlock);
    epicsThreadSleep(0.1);
}
}


MAIN(epicsThreadTest)
{
    testPlan(8);

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
    epicsThreadSleep(3.0);

    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);

    info infoA = {0};
    epicsThreadCreate("threadA", 50, stackSize, thread, &infoA);

    info infoB = {1};
    epicsThreadCreate("threadB", 50, stackSize, thread, &infoB);

    epicsThreadSleep(2.0);

    return testDone();
}
