/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsdThreadPriorityTest.cpp */

/* Author:  Marty Kraimer Date:    21NOV2005 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsExit.h"


typedef struct info {
    epicsEventId      waitForMaster;
    epicsEventId      waitForClient;
}info;

extern "C" {

static void client(void *arg)
{
    info *pinfo = (info *)arg;
    epicsThreadId idSelf = epicsThreadGetIdSelf();
    int pass;

    for(pass = 0 ; pass < 3 ; pass++) {
        epicsEventWaitStatus status;
        status = epicsEventWait(pinfo->waitForMaster);
        if(status!=epicsEventWaitOK) {
            printf("task %p epicsEventWait returned %d\n", idSelf,(int)status);
        }
        epicsEventSignal(pinfo->waitForClient);
    }
}

extern "C" void epicsThreadPriorityTest(void *)
{
    unsigned int stackSize;
    info *pinfo;
    epicsThreadId clientId;
    epicsThreadId myId;
    epicsEventWaitStatus status;

    myId = epicsThreadGetIdSelf();
    epicsThreadSetPriority(myId,50);
    pinfo = (info *)calloc(1,sizeof(info));
    pinfo->waitForMaster = epicsEventMustCreate(epicsEventEmpty);
    pinfo->waitForClient = epicsEventMustCreate(epicsEventEmpty);
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    clientId = epicsThreadCreate("client",50,stackSize,client,pinfo);
    epicsEventSignal(pinfo->waitForMaster);
    status = epicsEventWaitWithTimeout(pinfo->waitForClient,.1);
    if(status!=epicsEventWaitOK) {
        printf("epicsEventWaitWithTimeout failed. Why????\n");
        goto done;
    }
    epicsThreadSetPriority(clientId,20);
    /* expect that client will not be able to run */
    epicsEventSignal(pinfo->waitForMaster);
    status = epicsEventTryWait(pinfo->waitForClient);
    if(status!=epicsEventWaitTimeout) {
        printf("epicsEventTryWait did not return epicsEventWaitTimeout\n");
    } else {
         status = epicsEventWaitWithTimeout(pinfo->waitForClient,.1);
         if(status!=epicsEventWaitOK) {
             printf("epicsEventWaitWithTimeout failed. Why????\n");
             goto done;
         }
    }
    epicsThreadSetPriority(clientId,80);
    /* expect that client will be able to run */
    epicsEventSignal(pinfo->waitForMaster);
    status = epicsEventTryWait(pinfo->waitForClient);
    if(status==epicsEventWaitOK) {
        printf("Seems to support strict priority scheduling\n");
    } else {
        printf("Does not appear to support strict priority scheduling\n");
        status = epicsEventWaitWithTimeout(pinfo->waitForClient,.1);
        if(status!=epicsEventWaitOK) {
            printf("epicsEventWaitWithTimeout failed. Why????\n");
            goto done;
        }
    }
done:
    epicsThreadSleep(1.0);
}
} /* extern "C" */
