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
#include <unistd.h>

#include "ellLib.h"
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
    ELLNODE            node;
    pthread_t          tid;
    pthread_attr_t     attr;
    struct sched_param schedParam;
    THREADFUNC	       createFunc;
    void	       *createArg;
    semBinaryId        suspendSem;
    int		       isSuspended;
    unsigned int       osiPriority;
    char               *name;
} threadInfo;

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_key_t getpthreadInfo;
static semMutexId pthreadMutex;
static ELLLIST pthreadList;
static commonAttr *pcommonAttr = 0;

#define checkStatus(status,message) \
if((status))  {\
    errlogPrintf("%s error %s\n",(message),strerror((status))); }

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("%s  error %s\n",(message),strerror((status))); \
    cantProceed((method)); \
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
    return((int)oss);
}

static threadInfo * init_threadInfo(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    threadInfo *pthreadInfo;
    int status;

    pthreadInfo = callocMustSucceed(1,sizeof(threadInfo),"init_threadInfo");
    pthreadInfo->createFunc = funptr;
    pthreadInfo->createArg = parm;
    status = pthread_attr_init(&pthreadInfo->attr);
    checkStatusQuit(status,"pthread_attr_init","init_threadInfo");
    status = pthread_attr_setdetachstate(
        &pthreadInfo->attr, PTHREAD_CREATE_DETACHED);
    if(errVerbose) checkStatus(status,"pthread_attr_setdetachstate");
#if defined (_POSIX_THREAD_ATTR_STACKSIZE)
#if defined (OSITHREAD_USE_DEFAULT_STACK)
    stackSize = 0;
#endif
    status = pthread_attr_setstacksize(
        &pthreadInfo->attr, (size_t)stackSize);
    if(errVerbose) checkStatus(status,"pthread_attr_setstacksize");
#endif
    status = pthread_attr_setscope(&pthreadInfo->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatus(status,"pthread_attr_setscope");
    pthreadInfo->osiPriority = priority;
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    status = pthread_attr_getschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(errVerbose) checkStatus(status,"pthread_attr_getschedparam");
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status && errVerbose) {
         errlogPrintf("threadCreate: pthread_attr_setschedparam failed %s",
             strerror(status));
         errlogPrintf(" sched_priority %d\n",
             pthreadInfo->schedParam.sched_priority);
    }
    status = pthread_attr_setinheritsched(
        &pthreadInfo->attr,PTHREAD_EXPLICIT_SCHED);
    if(errVerbose) checkStatus(status,"pthread_attr_setinheritsched");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    pthreadInfo->suspendSem = semBinaryMustCreate(semFull);
    pthreadInfo->name = mallocMustSucceed(strlen(name)+1,"threadCreate");
    strcpy(pthreadInfo->name,name);
    return((threadId)pthreadInfo);
}

static void once(void)
{
    threadInfo *pthreadInfo;
    int status;

    pthread_key_create(&getpthreadInfo,0);
    pthreadMutex = semMutexMustCreate();
    ellInit(&pthreadList);
    pcommonAttr = callocMustSucceed(1,sizeof(commonAttr),"osdThread::once");
    status = pthread_attr_init(&pcommonAttr->attr);
    checkStatusQuit(status,"pthread_attr_init","osdThread::once");
    status = pthread_attr_setdetachstate(
        &pcommonAttr->attr, PTHREAD_CREATE_DETACHED);
    checkStatus(status,"pthread_attr_setdetachstate");
    status = pthread_attr_setscope(&pcommonAttr->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatus(status,"pthread_attr_setscope");
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    status = pthread_attr_getschedpolicy(
        &pcommonAttr->attr,&pcommonAttr->schedPolicy);
    checkStatus(status,"pthread_attr_getschedpolicy");
    status = pthread_attr_getschedparam(
        &pcommonAttr->attr,&pcommonAttr->schedParam);
    checkStatus(status,"pthread_attr_getschedparam");
    pcommonAttr->maxPriority = sched_get_priority_max(pcommonAttr->schedPolicy);
    if(pcommonAttr->maxPriority == -1) {
        pcommonAttr->maxPriority = pcommonAttr->schedParam.sched_priority;
        errlogPrintf("sched_get_priority_max failed set to %d\n",
            pcommonAttr->maxPriority);
    }
    pcommonAttr->minPriority = sched_get_priority_min(pcommonAttr->schedPolicy);
    if(pcommonAttr->minPriority == -1) {
        pcommonAttr->minPriority = pcommonAttr->schedParam.sched_priority;
        errlogPrintf("sched_get_priority_min failed set to %d\n",
            pcommonAttr->maxPriority);
    }
#else
    if(errVerbose) errlogPrintf("task priorities are not implemented\n");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    pthreadInfo = init_threadInfo("_main_",0,0,0,0);
    status = pthread_setspecific(getpthreadInfo,(void *)pthreadInfo);
    checkStatusQuit(status,"pthread_setspecific","start_routine");
    semMutexMustTake(pthreadMutex);
    ellAdd(&pthreadList,(ELLNODE *)pthreadInfo);
    semMutexGive(pthreadMutex);
}

static void * start_routine(void *arg)
{
    threadInfo *pthreadInfo = (threadInfo *)arg;
    int status;
    status = pthread_setspecific(getpthreadInfo,arg);
    checkStatusQuit(status,"pthread_setspecific","start_routine");

    semMutexMustTake(pthreadMutex);
    ellAdd(&pthreadList,(ELLNODE *)pthreadInfo);
    semMutexGive(pthreadMutex);

    (*pthreadInfo->createFunc)(pthreadInfo->createArg);

    semMutexMustTake(pthreadMutex);
    ellDelete(&pthreadList,(ELLNODE *)pthreadInfo);
    semMutexGive(pthreadMutex);

    semBinaryDestroy(pthreadInfo->suspendSem);
    status = pthread_attr_destroy(&pthreadInfo->attr);
    checkStatusQuit(status,"pthread_attr_destroy","start_routine");
    free(pthreadInfo->name);
    free(pthreadInfo);
    return(0);
}

#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif

unsigned int threadGetStackSize (threadStackSizeClass stackSizeClass)
{
#if defined (OSITHREAD_USE_DEFAULT_STACK)
    return 0;
#else
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
#endif /* OSITHREAD_USE_DEFAULT_STACK */
}

threadId threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    threadInfo *pthreadInfo;
    int status;

    status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","threadCreate");
    pthreadInfo = init_threadInfo(name,priority,stackSize,funptr,parm);
    status = pthread_create(&pthreadInfo->tid,
	&pthreadInfo->attr,start_routine,pthreadInfo);
    checkStatusQuit(status,"pthread_create","threadCreate");
    return((threadId)pthreadInfo);
}

void threadSuspendSelf()
{
    threadInfo *pthreadInfo;
    int status;

    status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","threadSuspendSelf");
    pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    if(!pthreadInfo) {
        errlogPrintf("threadSuspendSelf pthread_getspecific returned 0\n");
        while(1) {
            errlogPrintf("sleeping for 5 seconds\n");
            threadSleep(5.0);
        }
    }
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
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(errVerbose) checkStatus(status,"pthread_attr_setschedparam");
    status = pthread_setschedparam(
        pthreadInfo->tid,pcommonAttr->schedPolicy,&pthreadInfo->schedParam);
    if(errVerbose) checkStatus(status,"pthread_setschedparam");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
}

int threadIsEqual(threadId id1, threadId id2)
{
    threadInfo *p1 = (threadInfo *)id1;
    threadInfo *p2 = (threadInfo *)id2;
    return(pthread_equal(p1->tid,p2->tid));
}

int threadIsSuspended(threadId id) {
    threadInfo *pthreadInfo = (threadInfo *)id;
    return(pthreadInfo->isSuspended ? 1 : 0);
}

void threadSleep(double seconds)
{
    struct timespec delayTime;
    struct timespec remainingTime;
    double nanoseconds;

    delayTime.tv_sec = (time_t)seconds;
    nanoseconds = (seconds - (double)delayTime.tv_sec) *1e9;
    delayTime.tv_nsec = (long)nanoseconds;
    nanosleep(&delayTime,&remainingTime);
}

threadId threadGetIdSelf(void) {
    threadInfo *pthreadInfo;
    int status;

    status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","threadGetIdSelf");
    pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    return((threadId)pthreadInfo);
}

const char *threadGetNameSelf()
{
    threadInfo *pthreadInfo;
    int status;

    status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","threadGetNameSelf");
    pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    return(pthreadInfo->name);
}

void threadGetName(threadId id, char *name, size_t size)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    int status;

    status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","threadGetName");
    strncpy (name, pthreadInfo->name, size - 1);
    name[size-1] = '\0';
}

void threadShow (void)
{
    threadInfo *pthreadInfo;
    int status;

    status = pthread_once(&once_control,once);
    errlogPrintf ("        NAME       ID      PRI    STATE     WAIT\n");
    semMutexMustTake(pthreadMutex);
    for(pthreadInfo=(threadInfo *)ellFirst(&pthreadList); pthreadInfo;
	pthreadInfo=(threadInfo *)ellNext((ELLNODE *)pthreadInfo)) {
	errlogPrintf("%12.12s %8x %8d\n", pthreadInfo->name,(threadId)
		pthreadInfo,pthreadInfo->osiPriority);
    }
    semMutexGive(pthreadMutex);
}

threadPrivateId threadPrivateCreate(void)
{
    pthread_key_t *key;
    int status;

    key = callocMustSucceed(1,sizeof(pthread_key_t),"threadPrivateCreate");
    status = pthread_key_create(key,0);
    checkStatusQuit(status,"pthread_key_create","threadPrivateCreate");
    return((threadPrivateId)key);
}

void threadPrivateDelete(threadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    status = pthread_key_delete(*key);
    checkStatusQuit(status,"pthread_key_delete","threadPrivateDelete");
}

void threadPrivateSet (threadPrivateId id, void *value)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    status = pthread_setspecific(*key,value);
    checkStatusQuit(status,"pthread_setspecific","threadPrivateSet");
}

void *threadPrivateGet(threadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    void *value;

    value = pthread_getspecific(*key);
    if(!value)
        errlogPrintf("threadPrivateGet: pthread_getspecific returned 0\n");
    return(value);
}
