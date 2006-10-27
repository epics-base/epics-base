/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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


typedef struct info {
    epicsEventId terminate;
    epicsEventId terminated;
}info;

static void atExit(void *pvt)
{
    info *pinfo = (info *)pvt;
    epicsEventSignal(pinfo->terminate);
    /*Now wait for thread to terminate*/
    epicsEventMustWait(pinfo->terminated);
    epicsEventDestroy(pinfo->terminate);
    epicsEventDestroy(pinfo->terminated);
    free(pinfo);
}

static void thread(void *arg)
{
    info *pinfo = (info *)arg;

    printf("thread %s starting\n", epicsThreadGetNameSelf());
    pinfo->terminate = epicsEventMustCreate(epicsEventEmpty);
    pinfo->terminated = epicsEventMustCreate(epicsEventEmpty);
    epicsAtExit(atExit,pinfo);
    printf("thread %s waiting for atExit\n", epicsThreadGetNameSelf());
    epicsEventMustWait(pinfo->terminate);
    printf("thread %s terminating\n", epicsThreadGetNameSelf());
    epicsEventSignal(pinfo->terminated);
}
void epicsExitTest(void)
{
    unsigned int stackSize;
    info *pinfoA;
    info *pinfoB;

    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    pinfoA = (info *)calloc(1,sizeof(info));
    epicsThreadCreate("threadA",50,stackSize,thread,pinfoA);
    pinfoB = (info *)calloc(1,sizeof(info));
    epicsThreadCreate("threadB",50,stackSize,thread,pinfoB);
    epicsThreadSleep(1.0);
}
