/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* ringPointerTest.c */

/* Author:  Marty Kraimer Date:    13OCT2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "epicsThread.h"
#include "epicsRingPointer.h"
#include "errlog.h"
#include "epicsEvent.h"

#define ringSize 10

typedef struct info {
    epicsEventId consumerEvent;
    epicsRingPointerId	ring;
}info;

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    static int expectedValue=0;
    int *newvalue;

    printf("consumer starting\n");
    while(1) {
        epicsEventMustWait(pinfo->consumerEvent);
        while((newvalue = (int *)epicsRingPointerPop(pinfo->ring))) {
            if(expectedValue != *newvalue) {
                printf("consumer expected %d got %d\n",
                    expectedValue,*newvalue);
            }
            expectedValue = *newvalue + 1;
        }  
    }
}

void ringPointerTest()
{
    int i;
    info *pinfo;
    epicsEventId consumerEvent;
    int value[ringSize*2];
    int *pgetValue;
    epicsRingPointerId ring;

    for(i=0; i<ringSize*2; i++) value[i] = i;
    pinfo = calloc(1,sizeof(info));
    pinfo->consumerEvent = consumerEvent = epicsEventMustCreate(epicsEventEmpty);
    if(!consumerEvent) {printf("epicsEventMustCreate failed\n");exit(1);}
    pinfo->ring = ring = epicsRingPointerCreate(ringSize);
    if(!ring) {printf("epicsRingPointerCreate failed\n");exit(1);}
    epicsThreadCreate("consumer",50,epicsThreadGetStackSize(epicsThreadStackSmall),
        consumer,pinfo);
    if(!epicsRingPointerIsEmpty(ring)) printf("epicsRingPointerIsEmpty failed\n");
    printf("fill ring\n");
    i=0;
    while(1) {
        if(!epicsRingPointerPush(ring,(void *)&value[i])) break;
        ++i;
    }
    if(i!=ringSize) printf("fill ring failed i %d ringSize %d\n",i,ringSize);
    printf("empty ring\n");
    i=0;
    while(1) {
        if(!(pgetValue = (int *)epicsRingPointerPop(ring))) break;
        if(i!=*pgetValue) printf("main expected %d got %d\n",i,*pgetValue);
        ++i;
    }
    if(!epicsRingPointerIsEmpty(ring)) printf("epicsRingPointerIsEmpty failed\n");
    for(i=0; i<ringSize*2; i++) {
        while(epicsRingPointerIsFull(ring)) {
            epicsEventSignal(consumerEvent);
            epicsThreadSleep(2.0);
        }
        if(!epicsRingPointerPush(ring,(void *)&value[i]))
            printf("Why is ring full\n");
    }
    epicsEventSignal(consumerEvent);
    epicsThreadSleep(2.0);
}
