/* semMutexTest.c */

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
#include "errlog.h"


typedef struct info {
    int        threadnum;
    semMutexId mutex;
    int        quit;
}info;

static void mutexThread(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    printf("mutexThread %d starting time %ld\n",pinfo->threadnum,time(&tp));
    while(1) {
        semTakeStatus status;
        if(pinfo->quit) {
            printf("mutexThread %d returning time %ld\n",
                pinfo->threadnum,time(&tp));
            return;
        }
        status = semMutexTake(pinfo->mutex);
        if(status!=semTakeOK) {
            printf("task %d semMutexTake returned %d  time %ld\n",
                pinfo->threadnum,(int)status,time(&tp));
        }
        printf("mutexThread %d semMutexTake time %ld\n",
            pinfo->threadnum,time(&tp));
        threadSleep(.1);
        semMutexGive(pinfo->mutex);
        threadSleep(.9);
    }
}

void semMutexTest(int nthreads,int verbose)
{
    unsigned int stackSize;
    threadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    semMutexId mutex;
    int status;
    time_t tp;
    int errVerboseSave = errVerbose;

    errVerbose = verbose;
    mutex = semMutexMustCreate();
    printf("calling semMutexTake(mutex) time %ld\n",time(&tp));
    status = semMutexTake(mutex);
    if(status) printf("status %d\n",status);
    printf("calling semMutexTakeTimeout(mutex,2.0) time %ld\n",time(&tp));
    status = semMutexTakeTimeout(mutex,2.0);
    if(status) printf("status %d\n",status);
    printf("calling semMutexTakeNoWait(mutex) time %ld\n",time(&tp));
    status = semMutexTakeNoWait(mutex);
    if(status) printf("status %d\n",status);
    semMutexShow(mutex,1);
    printf("calling semMutexGive() time %ld\n",time(&tp));
    semMutexGive(mutex);
    printf("calling semMutexGive() time %ld\n",time(&tp));
    semMutexGive(mutex);
    printf("calling semMutexGive() time %ld\n",time(&tp));
    semMutexGive(mutex);
    semMutexShow(mutex,1);

    if(nthreads<=0) {
        errVerbose = errVerboseSave;
        return;
    }
    id = calloc(nthreads,sizeof(threadId));
    name = calloc(nthreads,sizeof(char *));
    arg = calloc(nthreads,sizeof(void *));
    pinfo = calloc(nthreads,sizeof(info *));
    stackSize = threadGetStackSize(threadStackSmall);
    for(i=0; i<nthreads; i++) {
        name[i] = calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        pinfo[i] = calloc(1,sizeof(info));
        pinfo[i]->threadnum = i;
        pinfo[i]->mutex = mutex;
        arg[i] = pinfo[i];
        id[i] = threadCreate(name[i],40,stackSize,mutexThread,arg[i]);
        printf("semTest created mutexThread %d id %p time %ld\n",
            i, id[i],time(&tp));
    }
    threadSleep(5.0);
    printf("semTest setting quit time %ld\n",time(&tp));
    for(i=0; i<nthreads; i++) {
        pinfo[i]->quit = 1;
    }
    threadSleep(2.0);
    errVerbose = errVerboseSave;
}
