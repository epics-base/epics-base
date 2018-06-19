/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsExitTest.cpp */

/* Author:  Marty Kraimer Date:    09JUL2004*/

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "epicsThread.h"
#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsUnitTest.h"
#include "testMain.h"


typedef struct info {
    char name[64];
    epicsEventId terminate;
    epicsEventId terminated;
}info;

static void atExit(void *pvt)
{
    info *pinfo = (info *)pvt;
    testPass("%s reached atExit", pinfo->name);
    epicsEventSignal(pinfo->terminate);
    /*Now wait for thread to terminate*/
    epicsEventMustWait(pinfo->terminated);
    testPass("%s destroying pinfo", pinfo->name);
    epicsEventDestroy(pinfo->terminate);
    epicsEventDestroy(pinfo->terminated);
    free(pinfo);
}

static void atThreadExit(void *pvt)
{
    info *pinfo = (info *)pvt;
    testPass("%s terminating", pinfo->name);
    epicsEventSignal(pinfo->terminated);
}

static void thread(void *arg)
{
    info *pinfo = (info *)arg;

    strcpy(pinfo->name, epicsThreadGetNameSelf());
    testDiag("%s starting", pinfo->name);
    pinfo->terminate = epicsEventMustCreate(epicsEventEmpty);
    pinfo->terminated = epicsEventMustCreate(epicsEventEmpty);
    testOk(!epicsAtExit(atExit, pinfo), "Registered atExit(%p)", pinfo);
    testOk(!epicsAtThreadExit(atThreadExit, pinfo),
        "Registered atThreadExit(%p)", pinfo);
    testDiag("%s waiting for atExit", pinfo->name);
    epicsEventMustWait(pinfo->terminate);
}

int count;

static void counter(void *pvt)
{
    count++;
}

static void mainExit(void *pvt)
{
    testPass("Reached mainExit");
    testDone();
}

MAIN(epicsExitTest)
{
    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    info *pinfoA = (info *)calloc(1, sizeof(info));
    info *pinfoB = (info *)calloc(1, sizeof(info));

    testPlan(15);

    testOk(!epicsAtExit(counter, NULL), "Registered counter()");
    count = 0;
    epicsExitCallAtExits();
    testOk(count == 1, "counter() called once");
    epicsExitCallAtExits();
    testOk(count == 1, "unregistered counter() not called");

    testOk(!epicsAtExit(mainExit, NULL), "Registered mainExit()");

    epicsThreadCreate("threadA", 50, stackSize, thread, pinfoA);
    epicsThreadSleep(0.1);
    epicsThreadCreate("threadB", 50, stackSize, thread, pinfoB);
    epicsThreadSleep(1.0);

    testDiag("Calling epicsExit");
    epicsExit(0);
    return 0;
}
