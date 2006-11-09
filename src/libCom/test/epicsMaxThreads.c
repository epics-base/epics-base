/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
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
#include "testMain.h"

static epicsEventId started;

static void thread(void *arg)
{
    epicsEventSignal(started);
    epicsThreadSuspendSelf();
}

MAIN(epicsMaxThreads)
{
    unsigned int  stackSize;
    epicsThreadId id;
    int           i = 0;

    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    printf("stackSize %d\n",stackSize);

    started = epicsEventMustCreate(epicsEventEmpty);

    while(1) {
        id = epicsThreadCreate("thread",50,stackSize,thread,0);
        if(!id) break;
        i++;
        if ((i % 100) == 0)
            printf ("Reached %d...\n", i);
        epicsEventMustWait(started);
    }

    printf("Max number of \"Small\" threads on this OS is %d\n", i);
    return 0;
}
