/* osiThread.c */

/* Author:  Marty Kraimer Date:    18JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

/* This is a posix implementation of osiThread */
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>

#include "osiThread.h"
#include "osiSem.h"
#include "cantProceed.h"
#include "errlog.h"

typedef struct commonAttr{
    pthread_attr_t     attr;
    struct sched_param schedParam;
    int                maxPriority;
    int                minPriority;
    int                schedPolicy;
} commonAttr;


typedef struct threadInfo {
    pthread_t          tid;
    pthread_attr_t     attr;
    struct sched_param schedParam;
    THREADFUNC	       createFunc;
    void	       *createArg;
    semBinaryId        suspendSem;
    int		       isSuspended;
    unsigned int       osiPriority;
} threadInfo;

static pthread_key_t getpthreadInfo;
static commonAttr *pcommonAttr = 0;

static void once(void)
{
    int status;

    pthread_key_create(&getpthreadInfo,0);
    pcommonAttr = callocMustSucceed(1,sizeof(commonAttr),"osdThread:once");
    status = pthread_attr_init(&pcommonAttr->attr);
    if(status) {
         errlogPrintf("pthread_attr_init failed: error %s\n",strerror(status));
         cantProceed("threadCreate::once");
    }
    status = pthread_attr_setdetachstate(
        &pcommonAttr->attr, PTHREAD_CREATE_DETACHED);
    if(status) {
         errlogPrintf("pthread_attr_setdetachstate1 failed: error %s\n",
             strerror(status));
    }
    status = pthread_attr_setscope(&pcommonAttr->attr,PTHREAD_SCOPE_PROCESS);
    if(status) {
         errlogPrintf("pthread_attr_setscope failed: error %s\n",
             strerror(status));
    }
    status = pthread_attr_getschedparam(
        &pcommonAttr->attr,&pcommonAttr->schedParam);
    if(status) {
         errlogPrintf("pthread_attr_getschedparam failed %s\n",
             strerror(status));
    }
    status = pthread_attr_getschedpolicy(
        &pcommonAttr->attr,&pcommonAttr->schedPolicy);
    if(status) {
         errlogPrintf("pthread_attr_getschedparam failed %s\n",
             strerror(status));
    }
    pcommonAttr->maxPriority = sched_get_priority_max(pcommonAttr->schedPolicy);
    pcommonAttr->minPriority = sched_get_priority_min(pcommonAttr->schedPolicy);
printf("schedPolicy %d maxPriority %d minPriority %d\n",
pcommonAttr->schedPolicy,
pcommonAttr->maxPriority,pcommonAttr->minPriority);
}

static void * start_routine(void *arg)
{
    threadInfo *pthreadInfo = (threadInfo *)arg;
    pthread_setspecific(getpthreadInfo,arg);
    (*pthreadInfo->createFunc)(pthreadInfo->createArg);
    semBinaryDestroy(pthreadInfo->suspendSem);
    pthread_attr_destroy(&pthreadInfo->attr);
    free(pthreadInfo);
    return(0);
}

static int getOssPriorityValue(threadInfo *pthreadInfo)
{
    double maxPriority,minPriority,slope,oss;

    if(pcommonAttr->maxPriority==pcommonAttr->minPriority)
        return(pcommonAttr->maxPriority);
    maxPriority = (double)pcommonAttr->maxPriority;
    minPriority = (double)pcommonAttr->minPriority;
    slope = (maxPriority - minPriority)/100.0;
    oss = (double)pthreadInfo->osiPriority * slope + minPriority;
printf("osiPriority %d osdPriority %f %d\n",
pthreadInfo->osiPriority,oss,(int)oss);
oss = pthreadInfo->osiPriority;
    return((int)oss);
}


#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif
/*
 * threadGetStackSize ()
 */
unsigned int threadGetStackSize (threadStackSizeClass stackSizeClass)
{
    static const unsigned stackSizeTable[threadStackBig+1] =
        {4000*ARCH_STACK_FACTOR, 6000*ARCH_STACK_FACTOR, 11000*ARCH_STACK_FACTOR};

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

threadId threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    threadInfo *pthreadInfo;
    pthread_t *ptid;
    int status;

    status = pthread_once(&once_control,once);
    pthreadInfo = callocMustSucceed(1,sizeof(threadInfo),"threadCreate");
    pthreadInfo->createFunc = funptr;
    pthreadInfo->createArg = parm;
    status = pthread_attr_init(&pthreadInfo->attr);
    if(status) {
         errlogPrintf("pthread_attr_init failed: error %s\n",strerror(status));
         cantProceed("threadCreate");
    }
    status = pthread_attr_setdetachstate(
        &pthreadInfo->attr, PTHREAD_CREATE_DETACHED);
    if(status) {
         errlogPrintf("pthread_attr_setdetachstate1 failed: error %s\n",
             strerror(status));
    }
    status = pthread_attr_setstacksize(
        &pthreadInfo->attr, (size_t)stackSize);
    if(status) {
         errlogPrintf("pthread_attr_setstacksize failed: error %s\n",
             strerror(status));
    }
    status = pthread_attr_setscope(&pthreadInfo->attr,PTHREAD_SCOPE_PROCESS);
    if(status) {
         errlogPrintf("pthread_attr_setscope failed: error %s\n",
             strerror(status));
    }
    pthreadInfo->osiPriority = priority;
    status = pthread_attr_getschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status) {
         errlogPrintf("threadCreate: pthread_attr_getschedparam failed %s\n",
             strerror(status));
    }
printf("sched_priority %d\n",pthreadInfo->schedParam.sched_priority);
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
pthreadInfo->schedParam.sched_priority = 10;
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status) {
         errlogPrintf("threadCreate: pthread_attr_setschedparam failed %s\n",
             strerror(status));
    }
    pthreadInfo->suspendSem = semBinaryMustCreate(semFull);
    status = pthread_create(&pthreadInfo->tid,
	&pthreadInfo->attr,start_routine,pthreadInfo);
    if(status) {
         errlogPrintf("pthread_create failed: error %s\n",strerror(status));
/*
         cantProceed("threadCreate");
*/
    }
    return((threadId)pthreadInfo);
}

void threadSuspend()
{
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    pthreadInfo->isSuspended = 1;
    semBinaryMustTake(pthreadInfo->suspendSem);
}

void threadResume(threadId id)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    pthreadInfo->isSuspended = 0;
    semBinaryGive(pthreadInfo->suspendSem);
}


unsigned int threadGetPriority(threadId id)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    return(pthreadInfo->osiPriority);
}

void threadSetPriority(threadId id,unsigned int priority)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    int status;

    pthreadInfo->osiPriority = priority;
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status) {
         errlogPrintf("threadSetPriority: pthread_attr_setschedparam failed %s\n",
             strerror(status));
    }
    status = pthread_setschedparam(
        pthreadInfo->tid,pcommonAttr->schedPolicy,&pthreadInfo->schedParam);
    if(status) {
         errlogPrintf("threadSetPriority: pthread_setschedparam failed %s\n",
             strerror(status));
    }
}

int threadIsEqual(threadId id1, threadId id2)
{
    threadInfo *p1 = (threadInfo *)id1;
    threadInfo *p2 = (threadInfo *)id2;
    return(pthread_equal(p1->tid,p2->tid));
}

int threadIsReady(threadId id) {
    threadInfo *pthreadInfo = (threadInfo *)id;
    return(pthreadInfo->isSuspended ? 1 : 0);
}

int threadIsSuspended(threadId id) {
    return(threadIsReady(id) ? 0 : 1);
}

void threadSleep(double seconds)
{
    struct timespec delayTime;
    struct timespec remainingTime;

    delayTime.tv_sec = (time_t)seconds;
    delayTime.tv_nsec = (long)(seconds - (double)delayTime.tv_sec);
    nanosleep(&delayTime,&remainingTime);
}

threadId threadGetIdSelf(void) {
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    return((threadId)pthreadInfo);
}
