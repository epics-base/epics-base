/* semBinaryTest.c */

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
#include <errno.h>
#include <time.h>

#include "osiThread.h"
#include "osiSem.h"
#include "osiRing.h"
#include "errlog.h"


typedef struct info {
    semBinaryId binary;
    semMutexId  lockRing;
    int         quit;
    ringId	ring;
}info;

static void consumer(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    threadId idSelf = threadGetIdSelf();

    printf("consumer %p starting time %ld\n",idSelf,time(&tp));
    while(1) {
        semTakeStatus status;
        if(pinfo->quit) {
            printf("consumer %p returning time %ld\n",
                idSelf,time(&tp));
            return;
        }
        status = semBinaryTake(pinfo->binary);
        if(status!=semTakeOK) {
            printf("task %p semBinaryTake returned %d  time %ld\n",
                idSelf,(int)status,time(&tp));
        }
        while(ringUsedBytes(pinfo->ring)>=2*sizeof(threadId)) {
            threadId message[2];
            int nget,i;

            for(i=0; i<2; i++) {
                nget = ringGet(pinfo->ring,(void *)&message[i],sizeof(threadId));
                if(nget!=sizeof(threadId))
                    printf("consumer error nget %d\n",nget);
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
    threadId idSelf = threadGetIdSelf();
    int ntimes=0;

    printf("producer %p starting time %ld\n",idSelf,time(&tp));
    while(1) {
        semTakeStatus status;

        ++ntimes;
        if(pinfo->quit) {
            printf("producer %p returning time %ld\n",
                idSelf,time(&tp));
            return;
        }
        status = semMutexTake(pinfo->lockRing);
        if(status!=semTakeOK) {
            printf("producer %p semMutexTake returned %d  time %ld\n",
                idSelf,(int)status,time(&tp));
        }
        if(ringFreeBytes(pinfo->ring)>=2*sizeof(int)) {
            int nput,i;

            for(i=0; i<2; i++) {
                nput = ringPut(pinfo->ring,(void *)&idSelf,sizeof(threadId));
                if(nput!=sizeof(threadId))
                    printf("producer %p error nput %d\n",idSelf,nput);
                if(i==0 && (ntimes%4==0)) threadSleep(.1);
            }
            printf("producer %p sending\n",idSelf);
        } else {
           printf("producer %p ring buffer is full\n",idSelf); 
        }
        semMutexGive(pinfo->lockRing);
        threadSleep(1.0);
        semBinaryGive(pinfo->binary);
    }
}

void semBinaryTest(int nthreads,int verbose)
{
    unsigned int stackSize;
    threadId *id;
    char **name;
    int i;
    info *pinfo;
    semBinaryId binary;
    int status;
    time_t tp;
    int errVerboseSave = errVerbose;

    errVerbose = verbose;
    binary = semBinaryMustCreate(semEmpty);
    printf("calling semBinaryTakeTimeout(binary,2.0) time %ld\n",time(&tp));
    status = semBinaryTakeTimeout(binary,2.0);
    if(status!=semTakeTimeout) printf("status %d\n",status);
    printf("calling semBinaryTakeNoWait(binary) time %ld\n",time(&tp));
    status = semBinaryTakeNoWait(binary);
    if(status!=semTakeTimeout) printf("status %d\n",status);
    printf("calling semBinaryGive() time %ld\n",time(&tp));
    semBinaryGive(binary);
    printf("calling semBinaryTakeTimeout(binary,2.0) time %ld\n",time(&tp));
    status = semBinaryTakeTimeout(binary,2.0);
    if(status) printf("status %d\n",status);
    printf("calling semBinaryGive() time %ld\n",time(&tp));
    semBinaryGive(binary);
    printf("calling semBinaryTakeNoWait(binary) time %ld\n",time(&tp));
    status = semBinaryTakeNoWait(binary);
    if(status) printf("status %d\n",status);

    if(nthreads<=0) {
        errVerbose = errVerboseSave;
        return;
    }
    pinfo = calloc(1,sizeof(info));
    pinfo->binary = binary;
    pinfo->lockRing = semMutexMustCreate();
    pinfo->ring = ringCreate(1024*2*sizeof(int));
    stackSize = threadGetStackSize(threadStackSmall);
    threadCreate("consumer",50,stackSize,consumer,pinfo);
    id = calloc(nthreads,sizeof(threadId));
    name = calloc(nthreads,sizeof(char *));
    for(i=0; i<nthreads; i++) {
        name[i] = calloc(10,sizeof(char));
        sprintf(name[i],"producer%d",i);
        id[i] = threadCreate(name[i],40,stackSize,producer,pinfo);
        printf("created producer %d id %p time %ld\n",
            i, id[i],time(&tp));
    }
    threadSleep(5.0);
    printf("semTest setting quit time %ld\n",time(&tp));
    pinfo->quit = 1;
    threadSleep(2.0);
    semBinaryGive(pinfo->binary);
    threadSleep(1.0);
    printf("semTest returning time %ld\n",time(&tp));
    errVerbose = errVerboseSave;
}
