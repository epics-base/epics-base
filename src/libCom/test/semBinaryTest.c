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
#include "errlog.h"


typedef struct info {
    int         threadnum;
    semBinaryId binary;
    int         quit;
}info;

static void binaryThread(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    printf("binaryThread %d starting time %d\n",pinfo->threadnum,time(&tp));
    threadSleep(1.0);
    while(1) {
        semTakeStatus status;
        if(pinfo->quit) {
            printf("binaryThread %d returning time %d\n",
                pinfo->threadnum,time(&tp));
            semBinaryGive(pinfo->binary);
            return;
        }
        status = semBinaryTake(pinfo->binary);
        if(status!=semTakeOK) {
            printf("task %d semBinaryTake returned %d  time %d\n",
                pinfo->threadnum,(int)status,time(&tp));
        }
        printf("binaryThread %d semBinaryTake time %d\n",
            pinfo->threadnum,time(&tp));
        semBinaryGive(pinfo->binary);
        threadSleep(1.0);
    }
}

void semBinaryTest(int nthreads)
{
    unsigned int stackSize;
    threadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    semBinaryId binary;
    int status;
    time_t tp;

    binary = semBinaryMustCreate(semEmpty);
    printf("calling semBinaryTakeTimeout(binary,2.0) time %d\n",time(&tp));
    status = semBinaryTakeTimeout(binary,2.0);
    if(status!=semTakeTimeout) printf("status %d\n",status);
    printf("calling semBinaryTakeNoWait(binary) time %d\n",time(&tp));
    status = semBinaryTakeNoWait(binary);
    if(status!=semTakeTimeout) printf("status %d\n",status);
    printf("calling semBinaryGive() time %d\n",time(&tp));
    semBinaryGive(binary);
    printf("calling semBinaryTakeTimeout(binary,2.0) time %d\n",time(&tp));
    status = semBinaryTakeTimeout(binary,2.0);
    if(status) printf("status %d\n",status);
    printf("calling semBinaryGive() time %d\n",time(&tp));
    semBinaryGive(binary);
    printf("calling semBinaryTakeNoWait(binary) time %d\n",time(&tp));
    status = semBinaryTakeNoWait(binary);
    if(status) printf("status %d\n",status);

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
        pinfo[i]->binary = binary;
        arg[i] = pinfo[i];
        id[i] = threadCreate(name[i],40,stackSize,binaryThread,arg[i]);
        printf("semTest created binaryThread %d id %p time %d\n",
            i, id[i],time(&tp));
    }
    threadSleep(2.0);
    printf("semTest calling semBinaryGive(binary) time %d\n",time(&tp));
    semBinaryGive(binary);
    threadSleep(5.0);
    printf("semTest setting quit time %d\n",time(&tp));
    for(i=0; i<nthreads; i++) {
        pinfo[i]->quit = 1;
    }
    semBinaryGive(binary);
    threadSleep(2.0);
}
