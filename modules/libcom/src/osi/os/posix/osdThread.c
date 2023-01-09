/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

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

#if (defined(_POSIX_MEMLOCK) && (_POSIX_MEMLOCK > 0) && !defined(__rtems__))
#  define USE_MEMLOCK 1
#else
#  define USE_MEMLOCK 0
#endif

#if USE_MEMLOCK
#include <sys/mman.h> 
#endif

/* epicsStdio uses epicsThreadOnce(), require explicit use to avoid unexpected recursion */
#define epicsStdioStdStreams
#define epicsStdioStdPrintfEtc

#include "epicsStdio.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "osdPosixMutexPriv.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsExit.h"
#include "epicsAtomic.h"

LIBCOM_API void epicsThreadShowInfo(epicsThreadOSD *pthreadInfo, unsigned int level);
LIBCOM_API void osdThreadHooksRun(epicsThreadId id);
LIBCOM_API void osdThreadHooksRunMain(epicsThreadId id);

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
    int                usePolicy;
} commonAttr;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
typedef struct {
    int min_pri, max_pri;
    int policy;
    int ok;
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
    errlogPrintf("%s " ERL_ERROR " %s\n",(message),strerror((status))); \
}

#define checkStatusQuit(status,message,method) \
if(status) { \
    errlogPrintf("%s  " ERL_ERROR " %s\n",(message),strerror((status))); \
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


LIBCOM_API int epicsThreadGetPosixPriority(epicsThreadId pthreadInfo)
{
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    double maxPriority,minPriority,slope,oss;

    if(pcommonAttr->maxPriority==pcommonAttr->minPriority)
        return(pcommonAttr->maxPriority);
    maxPriority = (double)pcommonAttr->maxPriority;
    minPriority = (double)pcommonAttr->minPriority;
    slope = (maxPriority - minPriority)/100.0;
    oss = (double)pthreadInfo->osiPriority * slope + minPriority;
    return((int)oss);
#else
    return 0;
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
}

static void setSchedulingPolicy(epicsThreadOSD *pthreadInfo,int policy)
{
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    int status;

    if(!pcommonAttr->usePolicy) return;

    status = pthread_attr_getschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    checkStatusOnce(status,"pthread_attr_getschedparam");
    pthreadInfo->schedParam.sched_priority = epicsThreadGetPosixPriority(pthreadInfo);
    pthreadInfo->schedPolicy = policy;
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

    /* sizeof(epicsThreadOSD) includes one byte for the '\0' */
    pthreadInfo = calloc(1,sizeof(*pthreadInfo) + strlen(name));
    if(!pthreadInfo)
        return NULL;
    pthreadInfo->suspendEvent = epicsEventCreate(epicsEventEmpty);
    if(!pthreadInfo->suspendEvent){
        free(pthreadInfo);
        return NULL;
    }
    strcpy(pthreadInfo->name, name);
    epicsAtomicIncrIntT(&pthreadInfo->refcnt); /* initial ref for the thread itself */
    return pthreadInfo;
}

static epicsThreadOSD * init_threadInfo(const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm,
    unsigned joinable)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    pthreadInfo = create_threadInfo(name);
    if(!pthreadInfo)
        return NULL;
    pthreadInfo->createFunc = funptr;
    pthreadInfo->createArg = parm;
    pthreadInfo->joinable = !!joinable; /* ensure 0 or 1 for later atomic compare+swap */
    status = pthread_attr_init(&pthreadInfo->attr);
    checkStatusOnce(status,"pthread_attr_init");
    if(status) return 0;
    if(!joinable){
        status = pthread_attr_setdetachstate(
            &pthreadInfo->attr, PTHREAD_CREATE_DETACHED);
        checkStatusOnce(status,"pthread_attr_setdetachstate");
    }
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

static void free_threadInfo(void* raw)
{
    epicsThreadOSD *pthreadInfo = raw;
    int status;

    if(epicsAtomicDecrIntT(&pthreadInfo->refcnt) > 0) return;

    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","free_threadInfo");
    if(pthreadInfo->isOnThreadList) ellDelete(&pthreadList,&pthreadInfo->node);
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","free_threadInfo");
    epicsEventDestroy(pthreadInfo->suspendEvent);
    status = pthread_attr_destroy(&pthreadInfo->attr);
    checkStatusQuit(status,"pthread_attr_destroy","free_threadInfo");
    free(pthreadInfo);
}

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
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
         * detect the insufficient permission (EPERM)
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
    prm->ok = 1;

    return 0;
}

static void findPriorityRange(commonAttr *a_p)
{
priAvailable arg;
pthread_t    id;
void         *dummy;
int          status;

    arg.policy = a_p->schedPolicy;
    arg.ok = 0;

    status = pthread_create(&id, 0, find_pri_range, &arg);
    checkStatusOnceQuit(status, "pthread_create","epicsThreadInit");

    status = pthread_join(id, &dummy);
    checkStatusOnceQuit(status, "pthread_join","epicsThreadInit");

    a_p->minPriority = arg.min_pri;
    a_p->maxPriority = arg.max_pri;
    a_p->usePolicy = arg.ok;
}
#endif

/* 0 - In the process which loads libCom.
 * 1 - In a newly fork()'d child process
 * 2 - In a child which has been warned
 */
static int childAfterFork;

static void childHook(void)
{
    epicsAtomicSetIntT(&childAfterFork, 1);
}

static void once(void)
{
    epicsThreadOSD *pthreadInfo;
    int status;

#ifdef __rtems__
    (void)childHook;
#else
    status = pthread_atfork(NULL, NULL, &childHook);
    checkStatusOnce(status, "pthread_atfork");
#endif

    checkStatusOnceQuit(pthread_key_create(&getpthreadInfo,&free_threadInfo),
                        "pthread_key_create","epicsThreadInit");
    status = osdPosixMutexInit(&onceLock,PTHREAD_MUTEX_DEFAULT);
    checkStatusOnceQuit(status,"osdPosixMutexInit","epicsThreadInit");
    status = osdPosixMutexInit(&listLock,PTHREAD_MUTEX_DEFAULT);
    checkStatusOnceQuit(status,"osdPosixMutexInit","epicsThreadInit");
    pcommonAttr = calloc(1,sizeof(commonAttr));
    if(!pcommonAttr) checkStatusOnceQuit(errno,"calloc","epicsThreadInit");
    status = pthread_attr_init(&pcommonAttr->attr);
    checkStatusOnceQuit(status,"pthread_attr_init","epicsThreadInit");
    status = pthread_attr_setdetachstate(
        &pcommonAttr->attr, PTHREAD_CREATE_DETACHED);
    checkStatusOnce(status,"pthread_attr_setdetachstate");
    status = pthread_attr_setscope(&pcommonAttr->attr,PTHREAD_SCOPE_PROCESS);
    if(errVerbose) checkStatusOnce(status,"pthread_attr_setscope");

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
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

    if (errVerbose) {
        fprintf(stderr, "LRT: min priority: %d max priority %d\n",
            pcommonAttr->minPriority, pcommonAttr->maxPriority);
    }

#else
    if(errVerbose) fprintf(stderr,"task priorities are not implemented\n");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

    pthreadInfo = init_threadInfo("_main_",0,epicsThreadGetStackSize(epicsThreadStackSmall),0,0,0);
    assert(pthreadInfo!=NULL);
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
    osdThreadHooksRunMain(pthreadInfo);
    epicsThreadOnceCalled = 1;
}

static void * start_routine(void *arg)
{
    epicsThreadOSD *pthreadInfo = (epicsThreadOSD *)arg;
    int status;
    sigset_t blockAllSig;

    sigfillset(&blockAllSig);
    pthread_sigmask(SIG_SETMASK,&blockAllSig,NULL);
    status = pthread_setspecific(getpthreadInfo,arg);
    checkStatusQuit(status,"pthread_setspecific","start_routine");
    status = mutexLock(&listLock);
    checkStatusQuit(status,"pthread_mutex_lock","start_routine");
    ellAdd(&pthreadList,&pthreadInfo->node);
    pthreadInfo->isOnThreadList = 1;
    status = pthread_mutex_unlock(&listLock);
    checkStatusQuit(status,"pthread_mutex_unlock","start_routine");
    osdThreadHooksRun(pthreadInfo);

    (*pthreadInfo->createFunc)(pthreadInfo->createArg);

    epicsExitCallAtThreadExits ();
    return(0);
}

static void epicsThreadInit(void)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    int status = pthread_once(&once_control,once);
    checkStatusQuit(status,"pthread_once","epicsThreadInit");

    if(epicsAtomicGetIntT(&childAfterFork)==1 &&  epicsAtomicCmpAndSwapIntT(&childAfterFork, 1, 2)==1) {
        fprintf(stderr, "Warning: Undefined Behavior!\n"
                        "         Detected use of epicsThread from child process after fork()\n");
    }
}

LIBCOM_API
void epicsThreadRealtimeLock(void)
{
#if USE_MEMLOCK
#ifndef RTEMS_LEGACY_STACK // seems to be part of libbsd?
    if (pcommonAttr->maxPriority > pcommonAttr->minPriority) {
        int status = mlockall(MCL_CURRENT | MCL_FUTURE);

        if (status) {
            const int err = errno;
            switch(err) {
#ifdef __linux__
            case ENOMEM:
                fprintf(stderr, "epicsThreadRealtimeLock "
                        "Warning: unable to lock memory.  RLIMIT_MEMLOCK is too small or missing CAP_IPC_LOCK\n");
                break;
            case EPERM:
                fprintf(stderr, "epicsThreadRealtimeLock "
                                "Warning: unable to lock memory.  missing CAP_IPC_LOCK\n");
                break;
#endif
            default:
                fprintf(stderr, "epicsThreadRealtimeLock "
                                "Warning: Unable to lock the virtual address space.\n"
                                "VM page faults may harm real-time performance. errno=%d\n",
                        err);
            }
        }
    }
#endif // LEGACY STACK
#endif
}

#if defined (OSITHREAD_USE_DEFAULT_STACK)
#define STACK_SIZE(f) (0)
#elif defined(_POSIX_THREAD_ATTR_STACKSIZE) && _POSIX_THREAD_ATTR_STACKSIZE > 0
    #define STACK_SIZE(f) (f * 0x10000 * sizeof(void *))
    static const unsigned stackSizeTable[epicsThreadStackBig+1] = {
        STACK_SIZE(1), STACK_SIZE(2), STACK_SIZE(4)
    };
#else
#define STACK_SIZE(f) (0)
#endif /*_POSIX_THREAD_ATTR_STACKSIZE*/

LIBCOM_API unsigned int epicsStdCall epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass)
{
#if defined (OSITHREAD_USE_DEFAULT_STACK)
    return 0;
#elif defined(_POSIX_THREAD_ATTR_STACKSIZE) && _POSIX_THREAD_ATTR_STACKSIZE > 0
    if (stackSizeClass<epicsThreadStackSmall) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too small)");
        return stackSizeTable[epicsThreadStackBig];
    }

    if (stackSizeClass>epicsThreadStackBig) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too large)");
        return stackSizeTable[epicsThreadStackBig];
    }

    return stackSizeTable[stackSizeClass];
#else
    return 0;
#endif /*_POSIX_THREAD_ATTR_STACKSIZE*/
}

LIBCOM_API void epicsStdCall epicsThreadOnce(epicsThreadOnceId *id, void (*func)(void *), void *arg)
{
    static const struct epicsThreadOSD threadOnceComplete;
    #define EPICS_THREAD_ONCE_DONE (void*)&threadOnceComplete
    int status;
    void *prev, *self;

    if(epicsAtomicGetPtrT((void**)id) == EPICS_THREAD_ONCE_DONE) {
        return; /* fast path.  global init already done.  No need to lock */
    }

    self = epicsThreadGetIdSelf();
    status = mutexLock(&onceLock);
    if(status) {
        fprintf(stderr,"epicsThreadOnce: pthread_mutex_lock returned %s.\n",
            strerror(status));
        exit(-1);
    }

    /* slow path, check again and attempt to begin */
    prev = epicsAtomicCmpAndSwapPtrT((void**)id, EPICS_THREAD_ONCE_INIT, self);

    if (prev != EPICS_THREAD_ONCE_DONE) {
        if (prev == EPICS_THREAD_ONCE_INIT) { /* first call, already marked active */
            status = pthread_mutex_unlock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_unlock", "epicsThreadOnce");
            func(arg);
            status = mutexLock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_lock", "epicsThreadOnce");
            epicsAtomicSetPtrT((void**)id, EPICS_THREAD_ONCE_DONE);    /* mark done */

        } else if (prev == self) {
            status = pthread_mutex_unlock(&onceLock);
            checkStatusQuit(status,"pthread_mutex_unlock", "epicsThreadOnce");
            cantProceed("Recursive epicsThreadOnce() initialization\n");

        } else
            while ((prev = epicsAtomicGetPtrT((void**)id)) != EPICS_THREAD_ONCE_DONE) {
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

epicsThreadId
epicsThreadCreateOpt(const char * name,
    EPICSTHREADFUNC funptr, void * parm, const epicsThreadOpts *opts )
{
    unsigned int stackSize;
    epicsThreadOSD *pthreadInfo;
    int status;
    sigset_t blockAllSig, oldSig;

    epicsThreadInit();
    assert(pcommonAttr);

    if (!opts) {
        static const epicsThreadOpts opts_default = EPICS_THREAD_OPTS_INIT;
        opts = &opts_default;
    }
    stackSize = opts->stackSize;
    if (stackSize <= epicsThreadStackBig)
        stackSize = epicsThreadGetStackSize(stackSize);

    sigfillset(&blockAllSig);
    pthread_sigmask(SIG_SETMASK, &blockAllSig, &oldSig);

    pthreadInfo = init_threadInfo(name, opts->priority, stackSize, funptr,
        parm, opts->joinable);
    if (pthreadInfo==0)
        return 0;

    pthreadInfo->isEpicsThread = 1;
    setSchedulingPolicy(pthreadInfo, SCHED_FIFO);
    pthreadInfo->isRealTimeScheduled = 1;

    if (pthreadInfo->joinable) {
        /* extra ref for epicsThreadMustJoin() */
        epicsAtomicIncrIntT(&pthreadInfo->refcnt);
    }

    status = pthread_create(&pthreadInfo->tid, &pthreadInfo->attr,
        start_routine, pthreadInfo);
    if (status==EPERM) {
        /* Try again without SCHED_FIFO*/
        free_threadInfo(pthreadInfo);

        pthreadInfo = init_threadInfo(name, opts->priority, stackSize,
            funptr, parm, opts->joinable);
        if (pthreadInfo==0)
            return 0;

        pthreadInfo->isEpicsThread = 1;
        status = pthread_create(&pthreadInfo->tid, &pthreadInfo->attr,
            start_routine, pthreadInfo);
    }
    checkStatusOnce(status, "pthread_create");
    if (status) {
        if (pthreadInfo->joinable) {
            /* release extra ref which would have been for epicsThreadMustJoin() */
            epicsAtomicDecrIntT(&pthreadInfo->refcnt);
        }

        free_threadInfo(pthreadInfo);
        return 0;
    }

    status = pthread_sigmask(SIG_SETMASK, &oldSig, NULL);
    checkStatusOnce(status, "pthread_sigmask");
    return pthreadInfo;
}

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
    assert(pthreadInfo);
    pthreadInfo->tid = tid;
    pthreadInfo->osiPriority = 0;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    if(pthread_getschedparam(tid,&pthreadInfo->schedPolicy,&pthreadInfo->schedParam) == 0) {
        if ( pcommonAttr->usePolicy && pthreadInfo->schedPolicy == pcommonAttr->schedPolicy ) {
            pthreadInfo->osiPriority =
                 (pthreadInfo->schedParam.sched_priority - pcommonAttr->minPriority) * 100.0 /
                    (pcommonAttr->maxPriority - pcommonAttr->minPriority + 1);
        }
    }
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

    status = pthread_setspecific(getpthreadInfo,(void *)pthreadInfo);
    checkStatus(status,"pthread_setspecific createImplicit");
    if(status){
        free_threadInfo(pthreadInfo);
        return NULL;
    }
    return pthreadInfo;
}

void epicsThreadMustJoin(epicsThreadId id)
{
    void *ret = NULL;
    int status;

    if(!id) {
        return;
    } else if(epicsAtomicCmpAndSwapIntT(&id->joinable, 1, 0)!=1) {
        if(epicsThreadGetIdSelf()==id) {
            errlogPrintf("Warning: %s thread self-join of unjoinable\n", id->name);

        } else {
            /* try to error nicely, however in all likelihood de-ref of
             * 'id' has already caused SIGSEGV as we are racing thread exit,
             * which free's 'id'.
             */
            cantProceed("Error: %s thread not joinable.\n", id->name);
        }
        return;
    }

    status = pthread_join(id->tid, &ret);
    if(status == EDEADLK) {
        /* Thread can't join itself (directly or indirectly)
         * so we detach instead.
         */
        status = pthread_detach(id->tid);
        checkStatusOnce(status, "pthread_detach");
    } else checkStatusOnce(status, "pthread_join");
    free_threadInfo(id);
}

LIBCOM_API void epicsStdCall epicsThreadSuspendSelf(void)
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    pthreadInfo->isSuspended = 1;
    epicsEventWait(pthreadInfo->suspendEvent);
}

LIBCOM_API void epicsStdCall epicsThreadResume(epicsThreadOSD *pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    pthreadInfo->isSuspended = 0;
    epicsEventSignal(pthreadInfo->suspendEvent);
}

LIBCOM_API void epicsStdCall epicsThreadExitMain(void)
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();

    cantProceed("epicsThreadExitMain() must no longer be used.\n");

    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    if(pthreadInfo->createFunc) {
        errlogPrintf("called from non-main thread\n");
        cantProceed("epicsThreadExitMain");
    }
    else {
    pthread_exit(0);
    }
}

LIBCOM_API unsigned int epicsStdCall epicsThreadGetPriority(epicsThreadId pthreadInfo)
{
    assert(epicsThreadOnceCalled);
    return(pthreadInfo->osiPriority);
}

LIBCOM_API unsigned int epicsStdCall epicsThreadGetPrioritySelf(void)
{
    epicsThreadInit();
    return(epicsThreadGetPriority(epicsThreadGetIdSelf()));
}

LIBCOM_API void epicsStdCall epicsThreadSetPriority(epicsThreadId pthreadInfo,unsigned int priority)
{
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    int status;
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

    assert(epicsThreadOnceCalled);
    assert(pthreadInfo);
    if(!pthreadInfo->isEpicsThread) {
        /* not allowed to avoid dealing with (potentially) different scheduling
         * policies (FIFO vs. RR vs. OTHER vs. ...)
         */
        fprintf(stderr,"epicsThreadSetPriority called by non epics thread\n");
        return;
    }
    pthreadInfo->osiPriority = priority;
    if(!pthreadInfo->isRealTimeScheduled) return;

#if defined (_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    if(!pcommonAttr->usePolicy) return;
    pthreadInfo->schedParam.sched_priority = epicsThreadGetPosixPriority(pthreadInfo);
    status = pthread_attr_setschedparam(
        &pthreadInfo->attr,&pthreadInfo->schedParam);
    if(errVerbose) checkStatus(status,"pthread_attr_setschedparam");
    status = pthread_setschedparam(
        pthreadInfo->tid, pthreadInfo->schedPolicy, &pthreadInfo->schedParam);
    if(errVerbose) checkStatus(status,"pthread_setschedparam");
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */
}

LIBCOM_API epicsThreadBooleanStatus epicsStdCall epicsThreadHighestPriorityLevelBelow(
    unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
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

LIBCOM_API epicsThreadBooleanStatus epicsStdCall epicsThreadLowestPriorityLevelAbove(
    unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned newPriority = priority + 1;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
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

LIBCOM_API int epicsStdCall epicsThreadIsEqual(epicsThreadId p1, epicsThreadId p2)
{
    assert(epicsThreadOnceCalled);
    assert(p1);
    assert(p2);
    return(pthread_equal(p1->tid,p2->tid));
}

LIBCOM_API int epicsStdCall epicsThreadIsSuspended(epicsThreadId pthreadInfo) {
    assert(epicsThreadOnceCalled);
    assert(pthreadInfo);
    return(pthreadInfo->isSuspended ? 1 : 0);
}

LIBCOM_API void epicsStdCall epicsThreadSleep(double seconds)
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

LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetIdSelf(void) {
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    assert ( pthreadInfo );
    return(pthreadInfo);
}

LIBCOM_API pthread_t epicsThreadGetPosixThreadId ( epicsThreadId threadId )
{
    return threadId->tid;
}

LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetId(const char *name) {
    epicsThreadOSD *pthreadInfo;
    int status;

    assert(epicsThreadOnceCalled);
    status = mutexLock(&listLock);
    checkStatus(status,"pthread_mutex_lock epicsThreadGetId");
    if(status)
        return NULL;
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
    if(strcmp(name,pthreadInfo->name) == 0) break;
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatus(status,"pthread_mutex_unlock epicsThreadGetId");

    return(pthreadInfo);
}

LIBCOM_API const char epicsStdCall *epicsThreadGetNameSelf()
{
    epicsThreadOSD *pthreadInfo;

    epicsThreadInit();
    pthreadInfo = (epicsThreadOSD *)pthread_getspecific(getpthreadInfo);
    if(pthreadInfo==NULL)
        pthreadInfo = createImplicit();
    return(pthreadInfo->name);
}

LIBCOM_API void epicsStdCall epicsThreadGetName(epicsThreadId pthreadInfo, char *name, size_t size)
{
    assert(epicsThreadOnceCalled);
    strncpy(name, pthreadInfo->name, size-1);
    name[size-1] = '\0';
}

LIBCOM_API void epicsThreadMap(EPICS_THREAD_HOOK_ROUTINE func)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    epicsThreadInit();
    status = mutexLock(&listLock);
    checkStatus(status, "pthread_mutex_lock epicsThreadMap");
    if (status)
        return;
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while (pthreadInfo) {
        func(pthreadInfo);
        pthreadInfo = (epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatus(status, "pthread_mutex_unlock epicsThreadMap");
}

LIBCOM_API void epicsStdCall epicsThreadShowAll(unsigned int level)
{
    epicsThreadOSD *pthreadInfo;
    int status;

    epicsThreadInit();
    epicsThreadShow(0,level);
    status = mutexLock(&listLock);
    checkStatus(status,"pthread_mutex_lock epicsThreadShowAll");
    if(status)
        return;
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
        epicsThreadShowInfo(pthreadInfo,level);
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatus(status,"pthread_mutex_unlock epicsThreadShowAll");
}

LIBCOM_API void epicsStdCall epicsThreadShow(epicsThreadId showThread, unsigned int level)
{
    epicsThreadOSD *pthreadInfo;
    int status;
    int found = 0;

    epicsThreadInit();
    if(!showThread) {
        epicsThreadShowInfo(0,level);
        return;
    }
    status = mutexLock(&listLock);
    checkStatus(status,"pthread_mutex_lock epicsThreadShowAll");
    if(status)
        return;
    pthreadInfo=(epicsThreadOSD *)ellFirst(&pthreadList);
    while(pthreadInfo) {
        if (((epicsThreadId)pthreadInfo == showThread)
         || ((epicsThreadId)pthreadInfo->tid == showThread)) {
            found = 1;
            epicsThreadShowInfo(pthreadInfo,level);
        }
        pthreadInfo=(epicsThreadOSD *)ellNext(&pthreadInfo->node);
    }
    status = pthread_mutex_unlock(&listLock);
    checkStatus(status,"pthread_mutex_unlock epicsThreadShowAll");
    if(status) return;
    if (!found)
        epicsStdoutPrintf("Thread %#lx (%lu) not found.\n", (unsigned long)showThread, (unsigned long)showThread);
}

LIBCOM_API epicsThreadPrivateId epicsStdCall epicsThreadPrivateCreate(void)
{
    pthread_key_t *key;
    int status;

    epicsThreadInit();
    key = calloc(1,sizeof(pthread_key_t));
    if(!key)
        return NULL;
    status = pthread_key_create(key,0);
    checkStatus(status,"pthread_key_create epicsThreadPrivateCreate");
    if(status) {
        free(key);
        return NULL;
    }
    return((epicsThreadPrivateId)key);
}

LIBCOM_API void epicsStdCall epicsThreadPrivateDelete(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    status = pthread_key_delete(*key);
    checkStatusQuit(status,"pthread_key_delete","epicsThreadPrivateDelete");
    free((void *)key);
}

LIBCOM_API void epicsStdCall epicsThreadPrivateSet (epicsThreadPrivateId id, void *value)
{
    pthread_key_t *key = (pthread_key_t *)id;
    int status;

    assert(epicsThreadOnceCalled);
    if(errVerbose && !value)
        errlogPrintf("epicsThreadPrivateSet: setting value of 0\n");
    status = pthread_setspecific(*key,value);
    checkStatusQuit(status,"pthread_setspecific","epicsThreadPrivateSet");
}

LIBCOM_API void epicsStdCall *epicsThreadPrivateGet(epicsThreadPrivateId id)
{
    pthread_key_t *key = (pthread_key_t *)id;

    assert(epicsThreadOnceCalled);
    return pthread_getspecific(*key);
}

LIBCOM_API double epicsStdCall epicsThreadSleepQuantum ()
{
    double hz;
    hz = sysconf ( _SC_CLK_TCK );
    if(hz<=0)
        return 0.0;
    return 1.0 / hz;
}

LIBCOM_API int epicsThreadGetCPUs(void)
{
    long ret;
#ifdef _SC_NPROCESSORS_ONLN
    ret = sysconf(_SC_NPROCESSORS_ONLN);
    if (ret > 0)
        return ret;
#endif
#ifdef _SC_NPROCESSORS_CONF
    ret = sysconf(_SC_NPROCESSORS_CONF);
    if (ret > 0)
        return ret;
#endif
    return 1;
}
