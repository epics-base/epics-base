/* ringPointerTest.c */

/* Author:  Marty Kraimer Date:    13OCT2000 */

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
#include <errno.h>
#include <time.h>

#include "osiThread.h"
#include "epicsRingPointer.h"
#include "errlog.h"
#include "osiSem.h"

#define ringSize 10

typedef struct info {
    semBinaryId consumerEvent;
    epicsRingPointerId	ring;
}info;

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    static int expectedValue=0;
    int *newvalue;

    printf("consumer starting\n");
    while(1) {
        semTakeStatus status;
        status = semBinaryTake(pinfo->consumerEvent);
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
    semBinaryId consumerEvent;
    int value[ringSize*2];
    int *pgetValue;
    epicsRingPointerId ring;

    for(i=0; i<ringSize*2; i++) value[i] = i;
    pinfo = calloc(1,sizeof(info));
    pinfo->consumerEvent = consumerEvent = semBinaryMustCreate(semEmpty);
    if(!consumerEvent) {printf("semBinaryMustCreate failed\n");exit(1);}
    pinfo->ring = ring = epicsRingPointerCreate(ringSize);
    if(!ring) {printf("epicsRingPointerCreate failed\n");exit(1);}
    threadCreate("consumer",50,threadGetStackSize(threadStackSmall),
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
            semBinaryGive(consumerEvent);
            threadSleep(2.0);
        }
        if(!epicsRingPointerPush(ring,(void *)&value[i]))
            printf("Why is ring full\n");
    }
    semBinaryGive(consumerEvent);
    threadSleep(2.0);
}
