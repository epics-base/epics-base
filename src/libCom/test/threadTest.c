/* threadTest.c */

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


static void threadFunc(void *arg)
{
    int argvalue = *(int *)arg;
    errlogPrintf("threadFunc %d starting\n",argvalue);
    threadSleep(2.0);
    errlogPrintf("threadFunc %d stopping\n",argvalue);
}

void threadTest(int ntasks)
{
    unsigned int stackSize;
    threadId *id;
    int i;
    char **name;
    void **arg;

    id = calloc(ntasks,sizeof(threadId *));
    name = calloc(ntasks,sizeof(char **));
    arg = calloc(ntasks,sizeof(void *));
    errlogPrintf("threadTest starting\n");
    stackSize = threadGetStackSize(threadStackSmall);
    errlogPrintf("stackSize %u\n",stackSize);
    for(i=0; i<ntasks; i++) {
        int *argvalue;
        name[i] = calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        arg[i] = calloc(1,sizeof(int));
        argvalue = (int *)arg[i];
        *argvalue = i;
        id[i] = threadCreate(name[i],40+i,stackSize,threadFunc,arg[i]);
        errlogPrintf("threadTest created %d id %p\n",i,id[i]);
    }
    threadSleep(5.0);
}
