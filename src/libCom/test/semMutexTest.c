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
    int threadnum;
    semId mutex;
    int quit;
}info;

static void mutexThread(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    printf("mutexThread %d starting time %d\n",pinfo->threadnum,time(&tp));
    threadSleep(1.0);
    while(1) {
        semTakeStatus status;
        if(pinfo->quit) {
            printf("mutexThread %d returning time %d\n",
                pinfo->threadnum,time(&tp));
            semMutexGive(pinfo->mutex);
            return;
        }
        status = semMutexTake(pinfo->mutex);
        if(status!=semTakeOK) {
            printf("task %d semMutexTake returned %d  time %d\n",
                pinfo->threadnum,(int)status,time(&tp));
        }
        printf("mutexThread %d semMutexTake time %d\n",
            pinfo->threadnum,time(&tp));
        semMutexGive(pinfo->mutex);
        threadSleep(1.0);
    }
}

void semMutexTest(int nthreads)
{
    unsigned int stackSize;
    threadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    semId mutex;
    int status;
    time_t tp;

    mutex = semMutexMustCreate();
    printf("calling semMutexTake(mutex) time %d\n",time(&tp));
    status = semMutexTake(mutex);
    if(status) printf("status %d\n",status);
    printf("calling semMutexTakeTimeout(mutex,2.0) time %d\n",time(&tp));
    status = semMutexTakeTimeout(mutex,2.0);
    if(status) printf("status %d\n",status);
    printf("calling semMutexTakeNoWait(mutex) time %d\n",time(&tp));
    status = semMutexTakeNoWait(mutex);
    if(status) printf("status %d\n",status);
    semMutexShow(mutex);
    printf("calling semMutexGive() time %d\n",time(&tp));
    semMutexGive(mutex);
    printf("calling semMutexGive() time %d\n",time(&tp));
    semMutexGive(mutex);
    printf("calling semMutexGive() time %d\n",time(&tp));
    semMutexGive(mutex);
    semMutexShow(mutex);

    if(nthreads<=0) return;
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
        printf("semTest created mutexThread %d id %p time %d\n",
            i, id[i],time(&tp));
    }
    threadSleep(2.0);
    printf("semTest calling semMutexGive(mutex) time %d\n",time(&tp));
    semMutexGive(mutex);
    threadSleep(5.0);
    printf("semTest setting quit time %d\n",time(&tp));
    for(i=0; i<nthreads; i++) {
        pinfo[i]->quit = 1;
    }
    semMutexGive(mutex);
    threadSleep(2.0);
}
