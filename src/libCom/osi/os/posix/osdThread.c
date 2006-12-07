/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsThread.c */

/* Author:  Marty Kraimer Date:    18JAN2000 */

/* This is a posix implementation of epicsThread */
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>

#include "epicsStdio.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsExit.h"

static int mutexLock(pthread_mutex_t *id)
{
    int status;

    while(1) {
        status = pthread_mutex_lock(id);
        if(status!=EINTR) return status;
        errlogPrintf("pthread_mutex_lock returned EINTR. Violates SUSv3\n");
    }
}

#if defined DONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING
#undef _POSIX_THREAD_PRIORITY_SCHEDULING
#endif

typedef struct commonAttr{
    pthread_attr_t     attr;
    struct sched_param schedParam;
    int                maxPriority;
    int                minPriority;
    int                schedPolicy;
} commonAttr;

typedef struct epicsThreadOSD {
    ELLNODE            node;
    pthread_t          tid;
    pthread_attr_t     attr;
    struct sched_param schedParam;
    EPICSTHREADFUNC    createFunc;
    void              *createArg;
    epicsEventId       suspendEvent;
    int                isSuspended;
    int                isEpicsThread;
    int                isFifoScheduled;
    int                isOnThreadList;
    unsigned int       osiPriority;
    char              *name;
} epicsThreadOSD;

static pthread_key_t getpthreadInfo;
static pthread_mutex_t onceLock;
static pthread_mutex_t listLock;
static ELLLIST pthreadList;
static commonAttr *pcommonAttr = 0;
static int epicsThreadOnceCalled = 0;

static epicsThreadOSD *createImplicit(void);

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
    epicsThreadOSD *pthreadInfo;
    epicsThreadOSD *pthreadSelf;
    static int ntimes=0;
    int status;

    ntimes++;
    if(ntimes>1) {
        fprintf(stderr,"osdThread myAtExit extered multiple times\n");
        return;
    }
    epicsExitCallAtExits();
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","myAtExit");
    pthreadSelf = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadSelf==NULL)
        pthreadSelf = createImplicit();
    pthreadInfo=(epicsThreadOSD *)ellLast(&pthreadList);
    while(pthreadInfo) {
        if(pthreadInfo != pthreadSelf /*dont cancel this thread*/
        && (strcmp("_main_",pthreadInfo->name)!=0)){/* dont cancel main*/
            pthread_cancel(pthreadInfo->tid);
        }
        pthreadInfo=(epicsThreadOSD *)ellPrevious(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","myAtExit");

    /* delete all resources created by once */
    free(pcommonAttr); pcommonAttr=0;
    pthread_mutex_destroy(&listLock);
    pthread_mutex_destroy(&onceLock);
    pthread_key_delete(getpthreadInfo);
}

#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING)
static int getOssPriorityValue(epicsThreadOSD *pthreadInfo)
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
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
static void setSchedulingPolicy(epicsThreadOSD *pthreadInfo,int policy)
{
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING)
    int status;

    status = pthread_attr_getschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    checkStatusOnce(status,"pthread_attr_getschedparam");
    pthreadInfo->schedParam.sched_priority = getOssPriorityValue(pthreadInfo);
    status = pthread_attr_setschedpolicy(
        &pthreadInfo->attr,policy);
     if(status) {
         checkStatusOnce(status,"pthread_attr_setschedpolicy");
     }
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(status) {
         checkStatusOnce(status,"pthread_attr_setschedparam");
    }
    status = pthread_attr_setinheritsched(
        &pthreadInfo->attr,PTHREAD_EXPLICIT_SCHED);
    checkStatusOnce(status,"pthread_attr_setinheritsched");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
}

static epicsThreadOSD * create_threadInfo(const char *name)
{
    epicsThreadOSD *pthreadInfo;

    pthreadInfo = callocMustSucceed(1,sizeof(*pthreadInfo),"create_threadInfo");
    pthreadInfo->suspendEvent = epicsEventMustCreate(epicsEventEmpty);
    pthreadInfo->name = epicsStrDup(name);
    return pthreadInfo;
}

static epicsThreadOSD * init_threadInfo(const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    pthreadInfo = create_threadInfo(name);
    pthreadInfo->createFunc = funptr;
    pthreadInfo->createArg = parm;
    status = pthread_attr_init(&pthreadInfo->attr);
    checkStatusOnce(status,"pthread_attr_init");
    if(status) return 0;
    status = pthread_attr_setdetachstate(
        &pthreadInfo->attr, PTHREAD_CREATE_DETACHED);
    checkStatusOnce(status,"pthread_attr_setdetachstate");
#if defined (_POSIX_THREAD_ATTR_STACKSIZE)
#if ! defined (OSITHREAD_USE_DEFAULT_STACK)
    status = pthread_attr_setstacksize( &pthreadInfo->attr,(size_t)stackSize);
    checkStatusOnce(status,"pthread_attr_setstacksize");
#endif /*OSITHREAD_USE_DEFAULT_STACK*/
#endif /*_POSIX_THREAD_ATTR_STACKSIZE*/
    status = pthread_attr_setscope(&pthreadInfo->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setscope");
    pthreadInfo->osiPriority = priority;
    return(pthreadInfo);
}

static void free_threadInfo(epicsThreadOSD *pthreadInfo)
{
    int status;

    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","free_threadInfo");
    if(pthreadInfo->isOnThreadList) ellDelete(&pthreadList,&pthreadInfo->node);
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","free_threadInfo");
    epicsEventDestroy(pthreadInfo->suspendEvent);
    status = pthread_attr_destroy(&pthreadInfo->attr);
    checkStatusQuit(status,"pthread_attr_destroy","free_threadInfo");
    free(pthreadInfo->name);
    free(pthreadInfo);
}

static void once(void)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    pthread_key_create(&getpthreadInfo,0);
    status = pthread_mutex_init(&onceLock,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsThreadInit");
    status = pthread_mutex_init(&listLock,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsThreadInit");
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
    status = pthread_attr_setschedpolicy(
        &pcommonAttr->attr,SCHED_FIFO);
    checkStatusOnce(status,"pthread_attr_setschedpolicy");
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
    pthreadInfo = init_threadInfo("_main_",0,epicsThreadGetStackSize(epicsThreadStackSmall),0,0);
    status = pthread_setspecific(getpthreadInfo,(void *)pthreadInfo);
    checkStatusOnceQuit(status,"pthread_setspecific","epicsThreadInit");
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsThreadInit");
    ellAdd(&pthreadList,&pthreadInfo->node);
    pthreadInfo->isOnThreadList = 1;
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadInit");
    status = atexit(myAtExit);
    checkStatusOnce(status,"atexit");
    epicsThreadOnceCalled = 1;
}

static void * start_routine(void *arg)
{
    epicsThreadOSD *pthreadInfo = (epicsThreadOSD *)arg;
    int status;
    int oldtype;
    sigset_t blockAllSig;
 
    sigfillset(&blockAllSig);
    pthread_sigmask(SIG_SETMASK,&blockAllSig,NULL);
    status = pthread_setspecific(getpthreadInfo,arg);
    checkStatusQuit(status,"pthread_setspecific","start_routine");
    status = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&oldtype);
    checkStatusQuit(status,"pthread_setcanceltype","start_routine");
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","start_routine");
    ellAdd(&pthreadList,&pthreadInfo->node);
    pthreadInfo->isOnThreadList = 1;
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","start_routine");

    (*pthreadInfo->createFunc)(pthreadInfo->createArg);

    epicsExitCallAtThreadExits ();

    free_threadInfo(pthreadInfo);
    return(0);
}

static void epicsThreadInit(void)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    int status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","epicsThreadInit");
}


#define ARCH_STACK_FACTOR 1024


unsigned int epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass)
{
#if ! defined (_POSIX_THREAD_ATTR_STACKSIZE)
    return 0;
#elif defined (OSITHREAD_USE_DEFAULT_STACK)
    return 0;
#else
    static const unsigned stackSizeTable[epicsThreadStackBig+1] =
        {128*ARCH_STACK_FACTOR, 256*ARCH_STACK_FACTOR, 512*ARCH_STACK_FACTOR};
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

/* epicsThreadOnce is a macro that calls epicsThreadOnceOsd */
void epicsThreadOnceOsd(epicsThreadOnceId *id, void (*func)(void *), void *arg)
{
    int status;
    epicsThreadInit();
    status = mutexLock(&onceLock);
    if(status) {
        fprintf(stderr,"epicsThreadOnceOsd epicsMutexLock failed.\n");
        exit(-1);
    }
    if (*id == 0) { /*  0 => first call */
        *id = -1;   /* -1 => func() active */
        /* avoid recursive locking */
        status = pthread_mutex_unlock(&onceLock);
        checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadOnceOsd");
        func(arg);
        status = mutexLock(&onceLock);
        checkStatusQuit(status,"pthread_mutex_lock","epicsThreadOnceOsd");
        *id = +1;   /* +1 => func() done */
    } else
        while (*id < 0) {
            /* Someone is in the above func(arg) call.  If that someone is
             * actually us, we're screwed, but the other OS implementations
             * will fire an assert() that should detect this condition.
             */
            status = pthread_mutex_unlock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadOnceOsd");
            epicsThreadSleep(0.01);
            status = mutexLock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_lock","epicsThreadOnceOsd");
        }
    status = pthread_mutex_unlock(&onceLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadOnceOsd");
}

epicsThreadId epicsThreadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm)
{
    epicsThreadOSD *pthreadInfo;
    int status;
    sigset_t blockAllSig, oldSig;

    epicsThreadInit();
    assert(pcommonAttr);
    sigfillset(&blockAllSig);
    pthread_sigmask(SIG_SETMASK,&blockAllSig,&oldSig);
    pthreadInfo = init_threadInfo(name,priority,stackSize,funptr,parm);
    if(pthreadInfo==0) return 0;
    pthreadInfo->isEpicsThread = 1;
    setSchedulingPolicy(pthreadInfo,SCHED_FIFO);
    pthreadInfo->isFifoScheduled = 1;
    status = pthread_create(&pthreadInfo->tid,&pthreadInfo->attr,
                start_routine,pthreadInfo);
    if(status==EPERM){
        /* Try again without SCHED_FIFO*/
        free_threadInfo(pthreadInfo);
        pthreadInfo = init_threadInfo(name,priority,stackSize,funptr,parm);
        if(pthreadInfo==0) return 0;
        pthreadInfo->isEpicsThread = 1;
        status = pthread_create(&pthreadInfo->tid,&pthreadInfo->attr,
                start_routine,pthreadInfo);
    }
    checkStatusOnce(status,"pthread_create");
    if(status) {
        free_threadInfo(pthreadInfo);
        return 0;
    }
    status = pthread_sigmask(SIG_SETMASK,&oldSig,NULL);
    checkStatusOnce(status,"pthread_sigmask");
    return(pthreadInfo);
}

/*
 * Cleanup routine for threads not created by epicsThreadCreate().
 */
/* static void nonEPICSthreadCleanup(void *arg)
{
    epicsThreadOSD *pthreadInfo = (epicsThreadOSD *)arg;

    free(pthreadInfo->name);
    free(pthreadInfo);
} */

/*
 * Create dummy context for threads not created by epicsThreadCreate().
 */
static epicsThreadOSD *createImplicit(void)
{
    epicsThreadOSD *pthreadInfo;
    char name[64];
    pthread_t tid;
    int status;

    tid = pthread_self();
    sprintf(name, "non-EPICS_%d", (int)tid);
    pthreadInfo = create_threadInfo(name);
    pthreadInfo->tid = tid;
    pthreadInfo->osiPriority = 0;
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    {
    struct sched_param param;
    int policy;
    if(pthread_getschedparam(tid,&policy,&param) == 0)
        pthreadInfo->osiPriority =
                 (param.sched_priority - pcommonAttr->minPriority) * 100.0 /
                    (pcommonAttr->maxPriority - pcommonAttr->minPriority + 1);
    }
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
    status = pthread_setspecific(getpthreadInfo,(void *)pthreadInfo);
    checkStatusQuit(status,"pthread_setspecific","createImplicit");
/*    pthread_cleanup_push(nonEPICSthreadCleanup, pthreadInfo); */
    return pthreadInfo;
}

void epicsThreadSuspendSelf(void)
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    pthreadInfo->isSuspended = 1;
    epicsEventMustWait(pthreadInfo->suspendEvent);
}

void epicsThreadResume(epicsThreadOSD *pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    pthreadInfo->isSuspended = 0;
    epicsEventSignal(pthreadInfo->suspendEvent);
}

void epicsThreadExitMain(void)
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    if(pthreadInfo->createFunc) {
        errlogPrintf("called from non-main thread\n");
        cantProceed("epicsThreadExitMain");
    }
    else {
    free_threadInfo(pthreadInfo);
    pthread_exit(0);
    }
}

unsigned int epicsThreadGetPriority(epicsThreadId pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    return(pthreadInfo->osiPriority);
}

unsigned int epicsThreadGetPrioritySelf(void)
{
    epicsThreadInit();
    return(epicsThreadGetPriority(epicsThreadGetIdSelf()));
}

void epicsThreadSetPriority(epicsThreadId pthreadInfo,unsigned int priority)
{
#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
    int status;
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

    assert(epicsThreadOnceCalled);
    assert(pthreadInfo);
    if(!pthreadInfo->isEpicsThread) {
        fprintf(stderr,"epicsThreadSetPriority called by non epics thread\n");
        return;
    }
    pthreadInfo->osiPriority = priority;
    if(!pthreadInfo->isFifoScheduled) return;
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

int epicsThreadIsEqual(epicsThreadId p1, epicsThreadId p2)
{
    assert(epicsThreadOnceCalled);
    assert(p1);
    assert(p2);
    return(pthread_equal(p1->tid,p2->tid));
}

int epicsThreadIsSuspended(epicsThreadId pthreadInfo) {
    assert(epicsThreadOnceCalled);
    assert(pthreadInfo);
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
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    assert ( pthreadInfo );
    return(pthreadInfo);
}

pthread_t epicsThreadGetPosixThreadId ( epicsThreadId threadId )
{
    return threadId->tid;
}

epicsThreadId epicsThreadGetId(const char *name) {
    epicsThreadOSD *pthreadInfo;
    int status;

    assert(epicsThreadOnceCalled);
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsThreadGetId");
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
    if(strcmp(name,pthreadInfo->name) == 0) break;
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadGetId");

    return(pthreadInfo);
}

const char *epicsThreadGetNameSelf()
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    return(pthreadInfo->name);
}

void epicsThreadGetName(epicsThreadId pthreadInfo, char *name, size_t size)
{
    assert(epicsThreadOnceCalled);
    strncpy(name, pthreadInfo->name, size-1);
    name[size-1] = '\0';
}

static void showThreadInfo(epicsThreadOSD *pthreadInfo,unsigned int level)
{
    if(!pthreadInfo) {
        fprintf(epicsGetStdout(),"            NAME     EPICS ID   "
            "PTHREAD ID   OSIPRI  OSSPRI  STATE\n");
    } else {
        struct sched_param param;
        int policy;
        int priority = 0;

        if(pthreadInfo->tid) {
            int status;
            status = pthread_getschedparam(pthreadInfo->tid,&policy,&param);
            if(!status) priority = param.sched_priority;
        }
        fprintf(epicsGetStdout(),"%16.16s %12p %12lu    %3d%8d %8.8s\n",
             pthreadInfo->name,(void *)
             pthreadInfo,(unsigned long)pthreadInfo->tid,
             pthreadInfo->osiPriority,priority,
             pthreadInfo->isSuspended?"SUSPEND":"OK");
    }
}

void epicsThreadShowAll(unsigned int level)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    epicsThreadInit();
    epicsThreadShow(0,level);
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsThreadShowAll");
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
        showThreadInfo(pthreadInfo,level);
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadShowAll");
}

void epicsThreadShow(epicsThreadId showThread, unsigned int level)
{
    epicsThreadOSD *pthreadInfo;
    int status;
    int found = 0;

    epicsThreadInit();
    if(!showThread) {
        showThreadInfo(0,level);
        return;
    }
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","epicsThreadShowAll");
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
        if (((epicsThreadId)pthreadInfo == showThread)
         || ((epicsThreadId)pthreadInfo->tid == showThread)) {
            found = 1;
            showThreadInfo(pthreadInfo,level);
        }
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadShowAll");
    if (!found)
        printf("Thread %#lx (%lu) not found.\n", (unsigned long)showThread, (unsigned long)showThread);
}


epicsThreadPrivateId epicsThreadPrivateCreate(void)
{
    pthread_key_t *key;
    int status;

    epicsThreadInit();
    key = callocMustSucceed(1,sizeof(pthread_key_t),"epicsThreadPrivateCreate");
    status = pthread_key_create(key,0);
    checkStatusQuit(status,"pthread_key_create","epicsThreadPrivateCreate");
    return((epicsThreadPrivateId)key);
}

void epicsThreadPrivateDelete(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    status = pthread_key_delete(*key);
    checkStatusQuit(status,"pthread_key_delete","epicsThreadPrivateDelete");
    free((void *)key);
}

void epicsThreadPrivateSet (epicsThreadPrivateId id, void *value)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    if(errVerbose && !value)
        errlogPrintf("epicsThreadPrivateSet: setting value of 0\n");
    status = pthread_setspecific(*key,value);
    checkStatusQuit(status,"pthread_setspecific","epicsThreadPrivateSet");
}

void *epicsThreadPrivateGet(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;

    assert(epicsThreadOnceCalled);
    return pthread_getspecific(*key);
}

double epicsThreadSleepQuantum ()
{
    double hz;
    hz = sysconf ( _SC_CLK_TCK );
    return 1.0 / hz;
}

