/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsEventTest.cpp */

/* Author:  Marty Kraimer Date:    26JAN2000 */
/*          timeout accuracy tests by Jeff Hill */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsRingPointer.h"
#include "epicsTime.h"
#include "errlog.h"


typedef struct info {
    epicsEventId      event;
    epicsMutexId      lockRing;
    int               quit;
    epicsRingPointerId ring;
}info;

extern "C" {

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    epicsThreadId idSelf = epicsThreadGetIdSelf();

    printf("consumer %p starting time %ld\n",idSelf,time(&tp));
    while(1) {
        epicsEventWaitStatus status;
        if(pinfo->quit) {
            printf("consumer %p returning time %ld\n",
                idSelf,time(&tp));
            return;
        }
        status = epicsEventWait(pinfo->event);
        if(status!=epicsEventWaitOK) {
            printf("task %p epicsEventWait returned %d  time %ld\n",
                idSelf,(int)status,time(&tp));
        }
        while(epicsRingPointerGetUsed(pinfo->ring)>=2) {
            epicsRingPointerId message[2];
            int i;

            for(i=0; i<2; i++) {
                if(!(message[i]=epicsRingPointerPop(pinfo->ring)))
                    printf("consumer error\n");
            }
            if(message[0]!=message[1]) {
                printf("consumer error message %p %p\n",message[0],message[1]);
            } else {
                printf("consumer message from %p\n",message[0]);
            }
        }  
    }
}

static void producer(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    epicsThreadId idSelf = epicsThreadGetIdSelf();
    int ntimes=0;

    printf("producer %p starting time %ld\n",idSelf,time(&tp));
    while(1) {
        epicsMutexLockStatus status;

        ++ntimes;
        if(pinfo->quit) {
            printf("producer %p returning time %ld\n",
                idSelf,time(&tp));
            return;
        }
        status = epicsMutexLock(pinfo->lockRing);
        if(status!=epicsMutexLockOK) {
            printf("producer %p epicsMutexLock returned %d  time %ld\n",
                idSelf,(int)status,time(&tp));
        }
        if(epicsRingPointerGetFree(pinfo->ring)>=2) {
            int i;

            for(i=0; i<2; i++) {
                if(!epicsRingPointerPush(pinfo->ring,idSelf))
                    printf("producer %p error\n",idSelf);
                if(i==0 && (ntimes%4==0)) epicsThreadSleep(.1);
            }
            printf("producer %p sending\n",idSelf);
        } else {
           printf("producer %p ring buffer is full\n",idSelf); 
        }
        epicsMutexUnlock(pinfo->lockRing);
        epicsThreadSleep(1.0);
        epicsEventSignal(pinfo->event);
    }
}

} // extern "C"

static double eventWaitMeasureDelayError( const epicsEventId &id, const double & delay )
{
    epicsTime beg = epicsTime::getCurrent();
    epicsEventWaitWithTimeout ( id, delay );
    epicsTime end = epicsTime::getCurrent();
    double meas = end - beg;
    double error = fabs ( delay - meas );
    printf ( "epicsEventWaitWithTimeout ( %10f ) tmo delay err %10f sec\n", 
        delay, error );
    return error;
}

static void eventWaitTest()
{
    double errorSum = 0.0;
    epicsEventId event = epicsEventMustCreate ( epicsEventEmpty );
    int i;
    for ( i = 0u; i < 20; i++ ) {
        double delay = ldexp ( 1.0 , -i );
        errorSum += eventWaitMeasureDelayError ( event, delay );
    }
    errorSum += eventWaitMeasureDelayError ( event, 0.0 );
    epicsEventDestroy ( event );
    printf ( "Average error %f sec\n", errorSum / ( i + 1 ) );
}


extern "C" void epicsEventTest(int nthreads,int verbose)
{
    unsigned int stackSize;
    epicsThreadId *id;
    char **name;
    int i;
    info *pinfo;
    epicsEventId event;
    int status;
    time_t tp;
    int errVerboseSave = errVerbose;

    eventWaitTest();

    errVerbose = verbose;
    event = epicsEventMustCreate(epicsEventEmpty);
    printf("calling epicsEventWaitWithTimeout(event,2.0) time %ld\n",time(&tp));
    status = epicsEventWaitWithTimeout(event,2.0);
    if(status!=epicsEventWaitTimeout) printf("status %d\n",status);
    printf("calling epicsEventTryWait(event) time %ld\n",time(&tp));
    status = epicsEventTryWait(event);
    if(status!=epicsEventWaitTimeout) printf("status %d\n",status);
    printf("calling epicsEventSignal() time %ld\n",time(&tp));
    epicsEventSignal(event);
    printf("calling epicsEventWaitWithTimeout(event,2.0) time %ld\n",time(&tp));
    status = epicsEventWaitWithTimeout(event,2.0);
    if(status) printf("status %d\n",status);
    printf("calling epicsEventSignal() time %ld\n",time(&tp));
    epicsEventSignal(event);
    printf("calling epicsEventTryWait(event) time %ld\n",time(&tp));
    status = epicsEventTryWait(event);
    if(status) printf("status %d\n",status);

    if(nthreads<=0) {
        errVerbose = errVerboseSave;
        return;
    }
    pinfo = (info *)calloc(1,sizeof(info));
    pinfo->event = event;
    pinfo->lockRing = epicsMutexCreate();
    pinfo->ring = epicsRingPointerCreate(1024*2);
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    epicsThreadCreate("consumer",50,stackSize,consumer,pinfo);
    id = (epicsThreadId *)calloc(nthreads,sizeof(epicsThreadId));
    name = (char **)calloc(nthreads,sizeof(char *));
    for(i=0; i<nthreads; i++) {
        name[i] = (char *)calloc(10,sizeof(char));
        sprintf(name[i],"producer%d",i);
        id[i] = epicsThreadCreate(name[i],40,stackSize,producer,pinfo);
        printf("created producer %d id %p time %ld\n",
            i, id[i],time(&tp));
    }
    epicsThreadSleep(5.0);
    printf("semTest setting quit time %ld\n",time(&tp));
    pinfo->quit = 1;
    epicsThreadSleep(2.0);
    epicsEventSignal(pinfo->event);
    epicsThreadSleep(1.0);
    printf("semTest returning time %ld\n",time(&tp));
    errVerbose = errVerboseSave;
}
