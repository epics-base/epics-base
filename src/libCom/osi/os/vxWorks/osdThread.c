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
static void **papTSD = 0;
static int nthreadPrivate = 0;

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

    taskVarAdd(tid,(int *)&papTSD);
    (*func)(parm);
    taskVarDelete(tid,(int *)&papTSD);
    free(papTSD);
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

void threadSuspendSelf()
{
    STATUS status;

    status = taskSuspend(taskIdSelf());
    if(status) errlogPrintf("threadSuspendSelf failed\n");
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

const char *threadGetNameSelf (void)
{
    return taskName(taskIdSelf());
}

void threadGetName (threadId id, char *name, size_t size)
{
    int tid = (int)id;
    strncpy(name,taskName(tid),size-1);
    name[size-1] = '\0';
}


/* The following algorithm was thought of by Andrew Johnson APS/ASD .
 * The basic idea is to use a single vxWorks task variable.
 * The task variable is papTSD, which is an array of pointers to the TSD
 * The array size is equal to the number of threadPrivateIds created + 1
 * when threadPrivateSet is called.
 * Until the first call to threadPrivateCreate by a application papTSD=0
 * After first call papTSD[0] is value of nthreadPrivate when 
 * threadPrivateSet was last called by the thread. This is also
 * the value of threadPrivateId.
 * The algorithm allows for threadPrivateCreate being called after
 * the first call to threadPrivateSet.
 */

threadPrivateId threadPrivateCreate()
{
    return((void *)++nthreadPrivate);
}

void threadPrivateDelete(threadPrivateId id)
{
    /*nothing to delete */
    return;
}

/*
 *note that it is not necessary to have mutex for following
 *because they must be called by the same thread
*/
void threadPrivateSet (threadPrivateId id, void *pvt)
{
    int indpthreadPrivate = (int)id;

    if(!papTSD) {
        papTSD = callocMustSucceed(indpthreadPrivate + 1,sizeof(void *),
            "threadPrivateSet");
        papTSD[0] = (void *)(indpthreadPrivate);
    } else {
        int nthreadPrivate = (int)papTSD[0];
        if(nthreadPrivate < indpthreadPrivate) {
            void **ptemp;
            ptemp = realloc(papTSD,(indpthreadPrivate+1)*sizeof(void *));
            if(!ptemp) cantProceed("threadPrivateSet realloc failed\n");
            papTSD = ptemp;
            papTSD[0] = (void *)(indpthreadPrivate);
        }
    }
    papTSD[indpthreadPrivate] = pvt;
}

void *threadPrivateGet(threadPrivateId id)
{
    assert(papTSD);
    assert((int)id <= (int)papTSD[0]);
    return(papTSD[(int)id]);
}
