/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* callbackTest.c */

/* Author:  Marty Kraimer Date:    26JAN2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "epicsThread.h"
#include "errlog.h"
#include "callback.h"
#include "taskwd.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct myPvt {
    CALLBACK callback;
    double   requestedDiff;
    epicsTimeStamp start;
}myPvt;

static void myCallback(CALLBACK *pCallback)
{
    myPvt *pmyPvt;
    epicsTimeStamp end;
    double diff, error;

    callbackGetUser(pmyPvt,pCallback);
    epicsTimeGetCurrent(&end);
    diff = epicsTimeDiffInSeconds(&end,&pmyPvt->start);
    error = fabs(pmyPvt->requestedDiff - diff);
    testOk(error < 0.05, "callback time error %f", error);
}
    
#define ncallbacks 5

MAIN(callbackTest)
{
    myPvt *nowait[ncallbacks];
    myPvt *wait[ncallbacks];
    epicsTimeStamp start;
    int i;

    testPlan(ncallbacks * 2);

    taskwdInit();
    errlogInit(4096);
    callbackInit();
    epicsThreadSleep(1.0);
    for(i=0; i<ncallbacks ; i++) {
        nowait[i] = calloc(1,sizeof(myPvt));
        callbackSetCallback(myCallback,&nowait[i]->callback);
        callbackSetUser(nowait[i],&nowait[i]->callback);
        callbackSetPriority(i%3,&nowait[i]->callback);
        epicsTimeGetCurrent(&start);
        nowait[i]->start = start;
        nowait[i]->requestedDiff = 0.0;
        callbackRequest(&nowait[i]->callback);
        wait[i] = calloc(1,sizeof(myPvt));
        callbackSetCallback(myCallback,&wait[i]->callback);
        callbackSetUser(wait[i],&wait[i]->callback);
        callbackSetPriority(i%3,&wait[i]->callback);
        epicsTimeGetCurrent(&start);
        wait[i]->start = start;
        wait[i]->requestedDiff = (double)i;
        callbackRequestDelayed(&wait[i]->callback,wait[i]->requestedDiff);
    }
    epicsThreadSleep((double)(ncallbacks + 2));
    return testDone();
}
