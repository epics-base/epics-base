/* epicsThread.c */

/* Author:  Marty Kraimer Date:    18JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

/* This is a posix implementation of epicsThread */
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
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsAssert.h"

/* Until these can be demonstrated to work leave them undefined*/
#undef _POSIX_THREAD_ATTR_STACKSIZE
#undef _POSIX_THREAD_PRIORITY_SCHEDULING

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
    EPICSTHREADFUNC    createFunc;
    void	       *createArg;
    epicsEventId        suspendEvent;
    int		       isSuspended;
    unsigned int       osiPriority;
    char               *name;
} threadInfo;

static pthread_key_t getpthreadInfo;
static epicsMutexId onceLock;
static epicsMutexId listLock;
static ELLLIST pthreadList;
static commonAttr *pcommonAttr = 0;
static int epicsThreadInitCalled = 0;

#define checkStatus(status,message) \
if((status))  {\
    errlogPrintf("%s error %s\n",(message),strerror((status))); \
}

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("%s  error %s\n",(message),strerror((status))); \
    cantProceed((method)); \
}

/* The following are for use by once, which is only invoked from epicsThreadInit*/
/* Until epicsThreadInit completes errlogInit will not work                     */
/* It must also be used by init_threadInfo otherwise errlogInit could get  */
/* called recursively                                                      */
#define checkStatusOnce(status,message) \
if((status))  {\
    fprintf(stderr,"%s error %s\n",(message),strerror((status))); }

#define checkStatusOnceQuit(status,message,method) \
if(status) { \
    fprintf(stderr,"%s  error %s",(message),strerror((status))); \
    fprintf(stderr," %s\n",method); \
    fprintf(stderr,"epicsThreadInit cant proceed. Program exiting\n"); \
    exit(-1);\
}

/* myAtExit just cancels all threads. Someday propercleanup is needed*/
static void myAtExit(void)
{
    threadInfo *pthreadInfo;
    static int ntimes=0;

    ntimes++;
    if(ntimes>1) {
        fprintf(stderr,"osdThread myAtExit extered multiple times\n");
        return;
    }
    epicsMutexMustLock(listLock);
    pthreadInfo=(threadInfo *)ellFirst(&pthreadList);
    while(pthreadInfo) {
        if(pthreadInfo->createFunc){/* dont cancel main thread*/
            pthread_cancel(pthreadInfo->tid);
        }
        pthreadInfo=(threadInfo *)ellNext(&pthreadInfo->node);
    }
    epicsMutexUnlock(listLock);
    /* delete all resources created by once */
    free(pcommonAttr); pcommonAttr=0;
    epicsMutexDestroy(listLock);
    epicsMutexDestroy(onceLock);
    pthread_key_delete(getpthreadInfo);
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
    EPICSTHREADFUNC funptr,void *parm)
{
    threadInfo *pthreadInfo;
    int status;

    pthreadInfo = callocMustSucceed(1,sizeof(threadInfo),"init_threadInfo");
    pthreadInfo->createFunc = funptr;
    pthreadInfo->createArg = parm;
    status = pthread_attr_init(&pthreadInfo->attr);
    checkStatusOnceQuit(status,"pthread_attr_init","init_threadInfo");
    status = pthread_attr_setdetachstate(
        &pthreadInfo->attr, PTHREAD_CREATE_DETACHED);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setdetachstate");
#if defined (_POSIX_THREAD_ATTR_STACKSIZE)
#if ! defined (OSITHREAD_USE_DEFAULT_STACK)
    status = pthread_attr_setstacksize(
        &pthreadInfo->attr, (size_t)stackSize);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setstacksize");
#endif /*OSITHREAD_USE_DEFAULT_STACK*/
#endif /*_POSIX_THREAD_ATTR_STACKSIZE*/
    status = pthread_attr_setscope(&pthreadInfo->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setscope");
    pthreadInfo->osiPriority = priority;
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    status = pthread_attr_getschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_getschedparam");
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status && errVerbose) {
         fprintf(stderr,"epicsThreadCreate: pthread_attr_setschedparam failed %s",
             strerror(status));
         fprintf(stderr," sched_priority %d\n",
             pthreadInfo->schedParam.sched_priority);
    }
    status = pthread_attr_setinheritsched(
        &pthreadInfo->attr,PTHREAD_EXPLICIT_SCHED);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setinheritsched");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    pthreadInfo->suspendEvent = epicsEventMustCreate(epicsEventEmpty);
    pthreadInfo->name = mallocMustSucceed(strlen(name)+1,"epicsThreadCreate");
    strcpy(pthreadInfo->name,name);
    return(pthreadInfo);
}

static void free_threadInfo(threadInfo *pthreadInfo)
{
    int status;

    epicsMutexMustLock(listLock);
    ellDelete(&pthreadList,&pthreadInfo->node);
    epicsMutexUnlock(listLock);
    epicsEventDestroy(pthreadInfo->suspendEvent);
    status = pthread_attr_destroy(&pthreadInfo->attr);
    checkStatusQuit(status,"pthread_attr_destroy","free_threadInfo");
    free(pthreadInfo->name);
    free(pthreadInfo);
}

static void once(void)
{
    threadInfo *pthreadInfo;
    int status;

    pthread_key_create(&getpthreadInfo,0);
    onceLock = epicsMutexMustCreate();
    listLock = epicsMutexMustCreate();
    ellInit(&pthreadList);
    pcommonAttr = calloc(1,sizeof(commonAttr));
    if(!pcommonAttr) checkStatusOnceQuit(errno,"calloc","epicsThreadInit");
    status = pthread_attr_init(&pcommonAttr->attr);
    checkStatusOnceQuit(status,"pthread_attr_init","epicsThreadInit");
    status = pthread_attr_setdetachstate(
        &pcommonAttr->attr, PTHREAD_CREATE_DETACHED);
    checkStatusOnce(status,"pthread_attr_setdetachstate");
    status = pthread_attr_setscope(&pcommonAttr->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setscope");
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    status = pthread_attr_getschedpolicy(
        &pcommonAttr->attr,&pcommonAttr->schedPolicy);
    checkStatusOnce(status,"pthread_attr_getschedpolicy");
    status = pthread_attr_getschedparam(
        &pcommonAttr->attr,&pcommonAttr->schedParam);
    checkStatusOnce(status,"pthread_attr_getschedparam");
    pcommonAttr->maxPriority = sched_get_priority_max(pcommonAttr->schedPolicy);
    if(pcommonAttr->maxPriority == -1) {
        pcommonAttr->maxPriority = pcommonAttr->schedParam.sched_priority;
        fprintf(stderr,"sched_get_priority_max failed set to %d\n",
            pcommonAttr->maxPriority);
    }
    pcommonAttr->minPriority = sched_get_priority_min(pcommonAttr->schedPolicy);
    if(pcommonAttr->minPriority == -1) {
        pcommonAttr->minPriority = pcommonAttr->schedParam.sched_priority;
        fprintf(stderr,"sched_get_priority_min failed set to %d\n",
            pcommonAttr->maxPriority);
    }
#else
    if(errVerbose) fprintf(stderr,"task priorities are not implemented\n");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    pthreadInfo = init_threadInfo("_main_",0,0,0,0);
    status = pthread_setspecific(getpthreadInfo,(void *)pthreadInfo);
    checkStatusOnceQuit(status,"pthread_setspecific","epicsThreadInit");
    status = epicsMutexLock(listLock);
    checkStatusOnceQuit(status,"epicsMutexLock","epicsThreadInit");
    ellAdd(&pthreadList,&pthreadInfo->node);
    epicsMutexUnlock(listLock);
    status = atexit(myAtExit);
    checkStatusOnce(status,"atexit");
}

static void * start_routine(void *arg)
{
    threadInfo *pthreadInfo = (threadInfo *)arg;
    int status;

    status = pthread_setspecific(getpthreadInfo,arg);
    checkStatusQuit(status,"pthread_setspecific","start_routine");
    status = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    checkStatusQuit(status,"pthread_setcanceltype","start_routine");
    epicsMutexMustLock(listLock);
    ellAdd(&pthreadList,&pthreadInfo->node);
    epicsMutexUnlock(listLock);

    (*pthreadInfo->createFunc)(pthreadInfo->createArg);

    free_threadInfo(pthreadInfo);
    return(0);
}

#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif

unsigned int epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass)
{
#if ! defined (_POSIX_THREAD_ATTR_STACKSIZE)
    return 0;
#elif defined (OSITHREAD_USE_DEFAULT_STACK)
    return 0;
#else
    static const unsigned stackSizeTable[epicsThreadStackBig+1] =
        {4000*ARCH_STACK_FACTOR, 6000*ARCH_STACK_FACTOR, 11000*ARCH_STACK_FACTOR};
    if (stackSizeClass<epicsThreadStackSmall) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too small)");
        return stackSizeTable[epicsThreadStackBig];
    }

    if (stackSizeClass>epicsThreadStackBig) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too large)");
        return stackSizeTable[epicsThreadStackBig];
    }

    return stackSizeTable[stackSizeClass];
#endif /*_POSIX_THREAD_ATTR_STACKSIZE*/
}

void epicsThreadInit(void)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    int status = pthread_once(&once_control,once);
    epicsThreadInitCalled = 1;
    checkStatusQuit(status,"pthread_once","epicsThreadInit");
}

/* epicsThreadOnce is a macro that calls epicsThreadOnceOsd */
void epicsThreadOnceOsd(epicsThreadOnceId *id, void (*func)(void *), void *arg)
{

    if(!epicsThreadInitCalled) epicsThreadInit();
    if(epicsMutexLock(onceLock) != epicsMutexLockOK) {
        fprintf(stderr,"epicsThreadOnceOsd epicsMutexLock failed.\n");
        fprintf(stderr,"Did you call epicsThreadInit? Program exiting\n");
        exit(-1);
    }
    if (*id == 0) { /*  0 => first call */
    	*id = -1;   /* -1 => func() active */
    	func(arg);
    	*id = +1;   /* +1 => func() done (see epicsThreadOnce() macro defn) */
    }
    epicsMutexUnlock(onceLock);
}

epicsThreadId epicsThreadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm)
{
    threadInfo *pthreadInfo;
    int status;

    if(!epicsThreadInitCalled) epicsThreadInit();
    assert(pcommonAttr);
    pthreadInfo = init_threadInfo(name,priority,stackSize,funptr,parm);
    status = pthread_create(&pthreadInfo->tid,&pthreadInfo->attr,
			    start_routine,pthreadInfo);
    checkStatusQuit(status,"pthread_create","epicsThreadCreate");
    return((epicsThreadId)pthreadInfo);
}

void epicsThreadSuspendSelf(void)
{
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    pthreadInfo->isSuspended = 1;
    epicsEventMustWait(pthreadInfo->suspendEvent);
}

void epicsThreadResume(epicsThreadId id)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    pthreadInfo->isSuspended = 0;
    epicsEventSignal(pthreadInfo->suspendEvent);
}

void epicsThreadExitMain(void)
{
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo->createFunc) {
        errlogPrintf("called from non-main thread\n");
        cantProceed("epicsThreadExitMain");
    }
    else {
	free_threadInfo(pthreadInfo);
	pthread_exit(0);
    }
}

unsigned int epicsThreadGetPriority(epicsThreadId id)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    return(pthreadInfo->osiPriority);
}

unsigned int epicsThreadGetPrioritySelf(void)
{
    return(epicsThreadGetPriority(epicsThreadGetIdSelf()));
}

void epicsThreadSetPriority(epicsThreadId id,unsigned int priority)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    int status;
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

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

epicsThreadBooleanStatus epicsThreadHighestPriorityLevelBelow(
    unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    int diff;
    diff = pcommonAttr->maxPriority - pcommonAttr->minPriority;
    if(diff<0) diff = -diff;
    if(diff>1 && diff <100) newPriority -=  100/(diff+1);
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    if (newPriority <= 99) {
        *pPriorityJustBelow = newPriority;
        return epicsThreadBooleanStatusSuccess;
    }
    return epicsThreadBooleanStatusFail;
}

epicsThreadBooleanStatus epicsThreadLowestPriorityLevelAbove(
    unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned newPriority = priority + 1;

#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    int diff;
    diff = pcommonAttr->maxPriority - pcommonAttr->minPriority;
    if(diff<0) diff = -diff;
    if(diff>1 && diff <100) newPriority += 100/(diff+1);
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    if (newPriority <= 99) {
        *pPriorityJustAbove = newPriority;
        return epicsThreadBooleanStatusSuccess;
    }
    return epicsThreadBooleanStatusFail;
}

int epicsThreadIsEqual(epicsThreadId id1, epicsThreadId id2)
{
    threadInfo *p1 = (threadInfo *)id1;
    threadInfo *p2 = (threadInfo *)id2;
    return(pthread_equal(p1->tid,p2->tid));
}

int epicsThreadIsSuspended(epicsThreadId id) {
    threadInfo *pthreadInfo = (threadInfo *)id;
    return(pthreadInfo->isSuspended ? 1 : 0);
}

void epicsThreadSleep(double seconds)
{
    struct timespec delayTime;
    struct timespec remainingTime;
    double nanoseconds;

    delayTime.tv_sec = (time_t)seconds;
    nanoseconds = (seconds - (double)delayTime.tv_sec) *1e9;
    delayTime.tv_nsec = (long)nanoseconds;
    nanosleep(&delayTime,&remainingTime);
}

epicsThreadId epicsThreadGetIdSelf(void) {
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    return((epicsThreadId)pthreadInfo);
}

epicsThreadId epicsThreadGetId(const char *name) {
    threadInfo *pthreadInfo;
    epicsMutexMustLock(listLock);
    pthreadInfo=(threadInfo *)ellFirst(&pthreadList);
    while(pthreadInfo) {
	if(strcmp(name,pthreadInfo->name) == 0) break;
        pthreadInfo=(threadInfo *)ellNext(&pthreadInfo->node);
    }
    epicsMutexUnlock(listLock);
    return((epicsThreadId)pthreadInfo);
}

const char *epicsThreadGetNameSelf()
{
    threadInfo *pthreadInfo = (threadInfo *)pthread_getspecific(getpthreadInfo);
    return(pthreadInfo->name);
}

void epicsThreadGetName(epicsThreadId id, char *name, size_t size)
{
    threadInfo *pthreadInfo = (threadInfo *)id;
    strncpy(name, pthreadInfo->name, size-1);
    name[size-1] = '\0';
}

void epicsThreadShowAll(unsigned int level)
{
    threadInfo *pthreadInfo;
    epicsThreadShow(0,level);
    epicsMutexMustLock(listLock);
    pthreadInfo=(threadInfo *)ellFirst(&pthreadList);
    while(pthreadInfo) {
	epicsThreadShow((epicsThreadId)pthreadInfo,level);
        pthreadInfo=(threadInfo *)ellNext(&pthreadInfo->node);
    }
    epicsMutexUnlock(listLock);
}

void epicsThreadShow(epicsThreadId id,unsigned int level)
{
    threadInfo *pthreadInfo = (threadInfo *)id;

    if(!id) {
	printf ("            NAME       ID   OSIPRI   OSSPRI    STATE\n");
    }
    else {
        struct sched_param param;
        int policy;
        int status;
        int priority;

        status = pthread_getschedparam(pthreadInfo->tid,&policy,&param);
        priority = (status ? 0 : param.sched_priority);
	printf("%16.16s %p %d %8d %8.8s\n", pthreadInfo->name,(epicsThreadId)
	       pthreadInfo,pthreadInfo->osiPriority,priority,pthreadInfo->
		     isSuspended?"SUSPEND":"OK");
	if(level>0)
	    ; /* more info */
    }
}


epicsThreadPrivateId epicsThreadPrivateCreate(void)
{
    pthread_key_t *key;
    int status;

    key = callocMustSucceed(1,sizeof(pthread_key_t),"epicsThreadPrivateCreate");
    status = pthread_key_create(key,0);
    checkStatusQuit(status,"pthread_key_create","epicsThreadPrivateCreate");
    return((epicsThreadPrivateId)key);
}

void epicsThreadPrivateDelete(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    status = pthread_key_delete(*key);
    checkStatusQuit(status,"pthread_key_delete","epicsThreadPrivateDelete");
}

void epicsThreadPrivateSet (epicsThreadPrivateId id, void *value)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    if(errVerbose && !value)
        errlogPrintf("epicsThreadPrivateSet: setting value of 0\n");
    status = pthread_setspecific(*key,value);
    checkStatusQuit(status,"pthread_setspecific","epicsThreadPrivateSet");
}

void *epicsThreadPrivateGet(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    void *value;

    value = pthread_getspecific(*key);
    if(errVerbose && !value)
        errlogPrintf("epicsThreadPrivateGet: pthread_getspecific returned 0\n");
    return(value);
}
