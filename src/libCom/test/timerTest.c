/* timerTest.c */

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
#include "osiTimer.h"
#include "errlog.h"
#include "epicsTime.h"

static void expire(void *pPrivate);
static void destroy(void *pPrivate);
static int again(void *pPrivate);
static double delay(void *pPrivate);
static void show(void *pPrivate, unsigned level);
static osiTimerJumpTable jumpTable = { expire,destroy,again,delay,show};
static osiTimerQueueId timerQueue;

typedef struct myPvt {
    osiTimerId timer;
    double   requestedDiff;
    epicsTimeStamp start;
}myPvt;

    
#define ntimers 3

void timerTest(void)
{
    myPvt *timer[ntimers];
    epicsTimeStamp start;
    int i;

    timerQueue = osiTimerQueueCreate(epicsThreadPriorityLow);
    for(i=0; i<ntimers ; i++) {
        timer[i] = calloc(1,sizeof(myPvt));
        timer[i]->timer = osiTimerCreate(&jumpTable,timerQueue,(void *)timer[i]);
        epicsTimeGetCurrent(&start);
        timer[i]->start = start;
        timer[i]->requestedDiff = (double)i;
        osiTimerArm(timer[i]->timer,(double)i);
    }
    epicsThreadSleep((double)(ntimers + 2));
    printf("timerTest returning\n");
}

void expire(void *pPrivate)
{
    myPvt *pmyPvt = (myPvt *)pPrivate;
    epicsTimeStamp end;
    double diff;

    epicsTimeGetCurrent(&end);
    diff = epicsTimeDiffInSeconds(&end,&pmyPvt->start);
    printf("myCallback requestedDiff %f diff %f\n",pmyPvt->requestedDiff,diff);
}

void destroy(void *pPrivate)
{
    return;
}

int again(void *pPrivate)
{
    return(0);
}

double delay(void *pPrivate)
{
    return(0.0);
}
void show(void *pPrivate, unsigned level)
{
    return;
}
