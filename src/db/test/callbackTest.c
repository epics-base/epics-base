/* callbackTest.c */

/* Author:  Marty Kraimer Date:    26JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "epicsThread.h"
#include "errlog.h"
#include "callback.h"
#include "taskwd.h"
#include "epicsTime.h"

typedef struct myPvt {
    CALLBACK callback;
    double   requestedDiff;
    epicsTimeStamp start;
}myPvt;

static void myCallback(CALLBACK *pCallback)
{
    myPvt *pmyPvt;
    epicsTimeStamp end;
    double diff;

    callbackGetUser(pmyPvt,pCallback);
    epicsTimeGetCurrent(&end);
    diff = epicsTimeDiffInSeconds(&end,&pmyPvt->start);
    printf("myCallback requestedDiff %f diff %f\n",pmyPvt->requestedDiff,diff);
}
    
#define ncallbacks 3

void callbackTest(void)
{
    myPvt *nowait[ncallbacks];
    myPvt *wait[ncallbacks];
    epicsTimeStamp start;
    int i;

    taskwdInit();
    errlogInit(4096);
    callbackInit();
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
    printf("callbackTest returning\n");
}
