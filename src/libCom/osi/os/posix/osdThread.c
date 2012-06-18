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

#define epicsExportSharedSymbols
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
        fprintf(stderr,"pthread_mutex_lock returned EINTR. Violates SUSv3\n");
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

#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
typedef struct {
    int min_pri, max_pri;
    int policy;
} priAvailable;
#endif

static pthread_key_t getpthreadInfo;
static pthread_mutex_t onceLock;
static pthread_mutex_t listLock;
static ELLLIST pthreadList = ELLLIST_INIT;
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
    checkStatusOnce(status,"pthread_attr_setschedpolicy");
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    checkStatusOnce(status,"pthread_attr_setschedparam");
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

#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) 
/*
 * The actually available range priority range (at least under linux)
 * may be restricted by resource limitations (but that is ignored
 * by sched_get_priority_max()). See bug #835138 which is fixed by
 * this code.
 */

static int try_pri(int pri, int policy)
{
struct sched_param  schedp;

    schedp.sched_priority = pri;
    return pthread_setschedparam(pthread_self(), policy, &schedp);
}

static void*
find_pri_range(void *arg)
{
priAvailable *prm = arg;
int           min = sched_get_priority_min(prm->policy);
int           max = sched_get_priority_max(prm->policy);
int           low, try;

    if ( -1 == min || -1 == max ) {
        /* something is very wrong; maintain old behavior
         * (warning message if sched_get_priority_xxx() fails
         * and use default policy's sched_priority [even if
         * that is likely to cause epicsThreadCreate to fail
         * because that priority is not suitable for SCHED_FIFO]).
         */
        prm->min_pri = prm->max_pri = -1;
        return 0;
    }


    if ( try_pri(min, prm->policy) ) {
        /* cannot create thread at minimum priority;
         * probably no permission to use SCHED_FIFO
         * at all. However, we still must return
         * a priority range accepted by the SCHED_FIFO
         * policy. Otherwise, epicsThreadCreate() cannot
         * detect the unsufficient permission (EPERM)
         * and fall back to a non-RT thread (because
         * pthread_attr_setschedparam would fail with
         * EINVAL due to the bad priority).
         */
        prm->min_pri = prm->max_pri = min;
        return 0;
    }


    /* Binary search through available priorities.
     * The actually available range may be restricted
     * by resource limitations (but that is ignored
     * by sched_get_priority_max() [linux]).
     */
    low = min;

    while ( low < max ) {
        try = (max+low)/2;
        if ( try_pri(try, prm->policy) ) {
            max = try;
        } else {
            low = try + 1;
        }
    }

    prm->min_pri = min;
    prm->max_pri = try_pri(max, prm->policy) ? max-1 : max;

    return 0;
}

static void findPriorityRange(commonAttr *a_p)
{
priAvailable arg;
pthread_t    id;
void         *dummy;
int          status;

    arg.policy = a_p->schedPolicy;

    status = pthread_create(&id, 0, find_pri_range, &arg);
    checkStatusQuit(status, "pthread_create","epicsThreadInit");

    status = pthread_join(id, &dummy);
    checkStatusQuit(status, "pthread_join","epicsThreadInit");

    a_p->minPriority = arg.min_pri;
    a_p->maxPriority = arg.max_pri;
}
#endif


static void once(void)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    pthread_key_create(&getpthreadInfo,0);
    status = pthread_mutex_init(&onceLock,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsThreadInit");
    status = pthread_mutex_init(&listLock,0);
    checkStatusQuit(status,"pthread_mutex_init","epicsThreadInit");
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

    findPriorityRange(pcommonAttr);

    if(pcommonAttr->maxPriority == -1) {
        pcommonAttr->maxPriority = pcommonAttr->schedParam.sched_priority;
        fprintf(stderr,"sched_get_priority_max failed set to %d\n",
            pcommonAttr->maxPriority);
    }
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
    status = atexit(epicsExitCallAtExits);
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


epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass)
{
#if ! defined (_POSIX_THREAD_ATTR_STACKSIZE)
    return 0;
#elif defined (OSITHREAD_USE_DEFAULT_STACK)
    return 0;
#else
    #define STACK_SIZE(f) (f * 0x10000 * sizeof(void *))
    static const unsigned stackSizeTable[epicsThreadStackBig+1] = {
        STACK_SIZE(1), STACK_SIZE(2), STACK_SIZE(4)
    };
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

epicsShareFunc void epicsShareAPI epicsThreadOnce(epicsThreadOnceId *id, void (*func)(void *), void *arg)
{
    static struct epicsThreadOSD threadOnceComplete;
    #define EPICS_THREAD_ONCE_DONE &threadOnceComplete
    int status;

    epicsThreadInit();
    status = mutexLock(&onceLock);
    if(status) {
        fprintf(stderr,"epicsThreadOnce: pthread_mutex_lock returned %s.\n",
            strerror(status));
        exit(-1);
    }

    if (*id != EPICS_THREAD_ONCE_DONE) {
        if (*id == EPICS_THREAD_ONCE_INIT) { /* first call */
            *id = epicsThreadGetIdSelf();    /* mark active */
            status = pthread_mutex_unlock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_unlock", "epicsThreadOnce");
            func(arg);
            status = mutexLock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_lock", "epicsThreadOnce");
            *id = EPICS_THREAD_ONCE_DONE;    /* mark done */
        } else if (*id == epicsThreadGetIdSelf()) {
            status = pthread_mutex_unlock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_unlock", "epicsThreadOnce");
            cantProceed("Recursive epicsThreadOnce() initialization\n");
        } else
            while (*id != EPICS_THREAD_ONCE_DONE) {
                /* Another thread is in the above func(arg) call. */
                status = pthread_mutex_unlock(&onceLock);
                checkStatusQuit(status,"pthread_mutex_unlock", "epicsThreadOnce");
                epicsThreadSleep(epicsThreadSleepQuantum());
                status = mutexLock(&onceLock);
                checkStatusQuit(status,"pthread_mutex_lock", "epicsThreadOnce");
            }
    }
    status = pthread_mutex_unlock(&onceLock);
    checkStatusQuit(status,"pthread_mutex_unlock","epicsThreadOnce");
}

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadCreate(const char *name,
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
    sprintf(name, "non-EPICS_%ld", (long)tid);
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

epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf(void)
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    pthreadInfo->isSuspended = 1;
    epicsEventMustWait(pthreadInfo->suspendEvent);
}

epicsShareFunc void epicsShareAPI epicsThreadResume(epicsThreadOSD *pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    pthreadInfo->isSuspended = 0;
    epicsEventSignal(pthreadInfo->suspendEvent);
}

epicsShareFunc void epicsShareAPI epicsThreadExitMain(void)
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

epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPriority(epicsThreadId pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    return(pthreadInfo->osiPriority);
}

epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPrioritySelf(void)
{
    epicsThreadInit();
    return(epicsThreadGetPriority(epicsThreadGetIdSelf()));
}

epicsShareFunc void epicsShareAPI epicsThreadSetPriority(epicsThreadId pthreadInfo,unsigned int priority)
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

epicsShareFunc epicsThreadBooleanStatus epicsShareAPI epicsThreadHighestPriorityLevelBelow(
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

epicsShareFunc epicsThreadBooleanStatus epicsShareAPI epicsThreadLowestPriorityLevelAbove(
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

epicsShareFunc int epicsShareAPI epicsThreadIsEqual(epicsThreadId p1, epicsThreadId p2)
{
    assert(epicsThreadOnceCalled);
    assert(p1);
    assert(p2);
    return(pthread_equal(p1->tid,p2->tid));
}

epicsShareFunc int epicsShareAPI epicsThreadIsSuspended(epicsThreadId pthreadInfo) {
    assert(epicsThreadOnceCalled);
    assert(pthreadInfo);
    return(pthreadInfo->isSuspended ? 1 : 0);
}

epicsShareFunc void epicsShareAPI epicsThreadSleep(double seconds)
{
    struct timespec delayTime;
    struct timespec remainingTime;
    double nanoseconds;

    if (seconds > 0) {
        delayTime.tv_sec = seconds;
        nanoseconds = (seconds - delayTime.tv_sec) *1e9;
        delayTime.tv_nsec = nanoseconds;
    }
    else {
        delayTime.tv_sec = 0;
        delayTime.tv_nsec = 0;
    }
    while (nanosleep(&delayTime, &remainingTime) == -1 &&
           errno == EINTR)
        delayTime = remainingTime;
}

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetIdSelf(void) {
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    assert ( pthreadInfo );
    return(pthreadInfo);
}

epicsShareFunc pthread_t epicsShareAPI epicsThreadGetPosixThreadId ( epicsThreadId threadId )
{
    return threadId->tid;
}

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetId(const char *name) {
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

epicsShareFunc const char epicsShareAPI *epicsThreadGetNameSelf()
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    return(pthreadInfo->name);
}

epicsShareFunc void epicsShareAPI epicsThreadGetName(epicsThreadId pthreadInfo, char *name, size_t size)
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

epicsShareFunc void epicsShareAPI epicsThreadShowAll(unsigned int level)
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

epicsShareFunc void epicsShareAPI epicsThreadShow(epicsThreadId showThread, unsigned int level)
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


epicsShareFunc epicsThreadPrivateId epicsShareAPI epicsThreadPrivateCreate(void)
{
    pthread_key_t *key;
    int status;

    epicsThreadInit();
    key = callocMustSucceed(1,sizeof(pthread_key_t),"epicsThreadPrivateCreate");
    status = pthread_key_create(key,0);
    checkStatusQuit(status,"pthread_key_create","epicsThreadPrivateCreate");
    return((epicsThreadPrivateId)key);
}

epicsShareFunc void epicsShareAPI epicsThreadPrivateDelete(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    status = pthread_key_delete(*key);
    checkStatusQuit(status,"pthread_key_delete","epicsThreadPrivateDelete");
    free((void *)key);
}

epicsShareFunc void epicsShareAPI epicsThreadPrivateSet (epicsThreadPrivateId id, void *value)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    if(errVerbose && !value)
        errlogPrintf("epicsThreadPrivateSet: setting value of 0\n");
    status = pthread_setspecific(*key,value);
    checkStatusQuit(status,"pthread_setspecific","epicsThreadPrivateSet");
}

epicsShareFunc void epicsShareAPI *epicsThreadPrivateGet(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;

    assert(epicsThreadOnceCalled);
    return pthread_getspecific(*key);
}

epicsShareFunc double epicsShareAPI epicsThreadSleepQuantum ()
{
    double hz;
    hz = sysconf ( _SC_CLK_TCK );
    return 1.0 / hz;
}

