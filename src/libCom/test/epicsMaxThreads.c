/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMaxThreads.cpp */

/* Author:  Marty Kraimer Date:    09JUL2004*/

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "errlog.h"

static epicsEventId started;

static void thread(void *arg)
{
    epicsEventSignal(started);
    epicsThreadSuspendSelf();
}

void epicsMaxThreads(void)
{
    unsigned int  stackSize;
    epicsThreadId id;
    int           i = 0;

    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    errlogPrintf("stackSize %d\n",stackSize);
    errlogFlush();
    started = epicsEventMustCreate(epicsEventEmpty);

    while(1) {
        id = epicsThreadCreate("thread",50,stackSize,thread,0);
        if(!id) break;
        i++;
        epicsEventMustWait(started);
    }
    fprintf(stdout,"number threads %d\n",i);
    epicsExitCallAtExits();
}
