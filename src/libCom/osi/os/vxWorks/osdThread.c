/* osi/os/vxWorks/osiThread.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <taskVarLib.h>
#include <sysLib.h>

#include "errlog.h"
#include "ellLib.h"
#include "osiThread.h"
#include "cantProceed.h"
#include "epicsAssert.h"

#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif
static const unsigned stackSizeTable[threadStackBig+1] = 
   {4000*ARCH_STACK_FACTOR, 6000*ARCH_STACK_FACTOR, 11000*ARCH_STACK_FACTOR};

/* definitions for implementation of threadPrivate */ 
typedef struct threadPrivateInfo {
    int nthreadPrivate;
    void **papTSD; /*pointer to array of pointers to thread specific data*/
}threadPrivateInfo;

static threadPrivateInfo *pthreadPrivateInfo = 0;
static int npthreadPrivate = 0;

/* Just map osi 0 to 99 into vx 100 to 199 */
/* remember that for vxWorks lower number means higher priority */
/* vx = 100 + (99 -osi)  = 199 - osi*/
/* osi =  199 - vx */

static unsigned int getOsiPriorityValue(int ossPriority)
{
    return(199-ossPriority);
}

static int getOssPriorityValue(unsigned int osiPriority)
{
    return(199 - osiPriority);
}


epicsShareFunc unsigned int epicsShareAPI threadGetStackSize (threadStackSizeClass stackSizeClass) 
{

    if (stackSizeClass<threadStackSmall) {
        errlogPrintf("threadGetStackSize illegal argument (too small)");
        return stackSizeTable[threadStackBig];
    }

    if (stackSizeClass>threadStackBig) {
        errlogPrintf("threadGetStackSize illegal argument (too large)");
        return stackSizeTable[threadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

static void createFunction(THREADFUNC func, void *parm)
{
    int tid = taskIdSelf();

    taskVarAdd(tid,(int *)&pthreadPrivateInfo);
    pthreadPrivateInfo = callocMustSucceed(
            1,sizeof(threadPrivateInfo),"threadPrivateSet");
    pthreadPrivateInfo->nthreadPrivate = 1;
    pthreadPrivateInfo->papTSD = callocMustSucceed(
        pthreadPrivateInfo->nthreadPrivate,
        sizeof(void *),"threadPrivateAlloc");
    (*func)(parm);
    taskVarDelete(tid,(int *)&pthreadPrivateInfo);
    free(pthreadPrivateInfo->papTSD);
    free(pthreadPrivateInfo);
}

threadId threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    int tid;
    if(stackSize<100) {
        errlogPrintf("threadCreate %s illegal stackSize %d\n",name,stackSize);
        return(0);
    }
    tid = taskSpawn((char *)name,getOssPriorityValue(priority),
        VX_FP_TASK, stackSize,
        (FUNCPTR)createFunction,(int)funptr,(int)parm,
        0,0,0,0,0,0,0,0);
    if(tid==0) {
        errlogPrintf("threadCreate taskSpawn failure for %s\n",name);
        return(0);
    }
    return((threadId)tid);
}

void threadSuspend()
{
    STATUS status;

    status = taskSuspend(taskIdSelf());
    if(status) errlogPrintf("threadSuspend failed\n");
}

void threadResume(threadId id)
{
    int tid = (int)id;
    STATUS status;

    status = taskResume(tid);
    if(status) errlogPrintf("threadResume failed\n");
}

unsigned int threadGetPriority(threadId id)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    status = taskPriorityGet(tid,&priority);
    if(status) errlogPrintf("threadGetPriority failed\n");
    return(getOsiPriorityValue(priority));
}

void threadSetPriority(threadId id,unsigned int osip)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    priority = getOssPriorityValue(osip);
    status = taskPrioritySet(tid,priority);
    if(status) errlogPrintf("threadSetPriority failed\n");
}

int threadIsEqual(threadId id1, threadId id2)
{
    return((id1==id2) ? 1 : 0);
}

int threadIsSuspended(threadId id)
{
    int tid = (int)id;
    return((int)taskIsSuspended(tid));
}

void threadSleep(double seconds)
{
    STATUS status;

    status = taskDelay((int)(seconds*sysClkRateGet()));
    if(status) errlogPrintf(0,"threadSleep\n");
}

threadId threadGetIdSelf(void)
{
    return((threadId)taskIdSelf());
}

/* The following algorithm was thought of by Andrew Johnson APS/ASD .
 * The basic idea is to use a single vxWorks task variable.
 * The task variable is pthreadPrivateInfo.
 * The variable papTSD is an array of pointers to the TSD.
 * The array size is equal to the number of threadVarIds created
 * when threadPrivateSet is called.
 * The algorithm allows for threadPrivateCreate being called after
 * the first call to threadPrivateSet.
 */


threadVarId threadPrivateCreate()
{
    return((void *)++npthreadPrivate);
}

void threadPrivateDelete(threadVarId id)
{
    /*not safe to delete anything*/
    return;
}

/*
 *note that it is not necessary to have mutex for following
 *because they must be called by the same thread
*/
void threadPrivateSet (threadVarId id, void *pvt)
{
    int indpthreadPrivate = (int)id;

    assert(pthreadPrivateInfo);
    if(pthreadPrivateInfo->nthreadPrivate <= indpthreadPrivate) {
        pthreadPrivateInfo->papTSD = realloc(pthreadPrivateInfo->papTSD,
            (indpthreadPrivate+1)*sizeof(void *));
        if(!pthreadPrivateInfo->papTSD)
            cantProceed("threadPrivateSet calloc failed\n");
        pthreadPrivateInfo->nthreadPrivate = indpthreadPrivate+1;
    }
    pthreadPrivateInfo->papTSD[indpthreadPrivate] = pvt;
    cantProceed("threadPrivateSet calloc failed\n");
}

void *threadPrivateGet(threadVarId id)
{
    assert(pthreadPrivateInfo);
    return(pthreadPrivateInfo->papTSD[(int)id]);
}
