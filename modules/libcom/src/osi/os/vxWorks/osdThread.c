/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/os/vxWorks/epicsThread.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <taskVarLib.h>
#include <sysLib.h>

#include "errlog.h"
#include "ellLib.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsAssert.h"
#include "vxLib.h"
#include "epicsExit.h"

#ifdef EPICS_THREAD_CAN_JOIN
  /* The implementation of epicsThreadMustJoin() here uses 2 features
   * of VxWorks that were first introduced in VxWorks 6.9: taskWait(),
   * and the taskSpareFieldGet/Set routines in taskUtilLib.
   */
  #include <taskUtilLib.h>

  #define JOIN_WARNING_TIMEOUT (60 * sysClkRateGet())

  static SPARE_NUM joinField;
  #define ALLOT_JOIN(tid) taskSpareNumAllot(tid, &joinField)
#else
  #define ALLOT_JOIN(tid)
#endif


LIBCOM_API void osdThreadHooksRun(epicsThreadId id);

#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif
static const unsigned stackSizeTable[epicsThreadStackBig+1] =
   {4000*ARCH_STACK_FACTOR, 6000*ARCH_STACK_FACTOR, 11000*ARCH_STACK_FACTOR};

/* Table and lock for epicsThreadMap() */
#define ID_LIST_CHUNK 512
static int *taskIdList = 0;
int taskIdListSize = 0;
static SEM_ID epicsThreadListMutex = 0;

/* This routine is found in atReboot.cpp */
extern void atRebootRegister(void);

/* definitions for implementation of epicsThreadPrivate */
static void **papTSD = 0;
static int nepicsThreadPrivate = 0;

static SEM_ID epicsThreadOnceMutex = 0;

/* Just map osi 0 to 99 into vx 100 to 199 */
/* remember that for vxWorks lower number means higher priority */
/* vx = 100 + (99 -osi)  = 199 - osi*/
/* osi =  199 - vx */

static unsigned int getOsiPriorityValue(int ossPriority)
{
    if ( ossPriority < 100 ) {
        return epicsThreadPriorityMax;
    }
    else if ( ossPriority > 199 ) {
        return epicsThreadPriorityMin;
    }
    else {
        return ( 199u - (unsigned int) ossPriority );
    }
}

static int getOssPriorityValue(unsigned int osiPriority)
{
    if ( osiPriority > 99 ) {
        return 100;
    }
    else {
        return ( 199 - (signed int) osiPriority );
    }
}

#define THREAD_SEM_FLAGS (SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY)
static void epicsThreadInit(void)
{
    static int lock = 0;
    static int done = 0;

    if (done) return;

    while(!vxTas(&lock))
        taskDelay(1);

    if (!done) {
        epicsThreadOnceMutex = semMCreate(THREAD_SEM_FLAGS);
        assert(epicsThreadOnceMutex);
        epicsThreadListMutex = semMCreate(THREAD_SEM_FLAGS);
        assert(epicsThreadListMutex);
        taskIdList = calloc(ID_LIST_CHUNK, sizeof(int));
        assert(taskIdList);
        taskIdListSize = ID_LIST_CHUNK;
        atRebootRegister();
        ALLOT_JOIN(0);
        done = 1;
    }
    lock = 0;
}

void epicsThreadRealtimeLock(void)
{}

unsigned int epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass)
{

    if (stackSizeClass<epicsThreadStackSmall) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too small)");
        return stackSizeTable[epicsThreadStackBig];
    }

    if (stackSizeClass>epicsThreadStackBig) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too large)");
        return stackSizeTable[epicsThreadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

struct epicsThreadOSD {};
    /* Strictly speaking this should be a WIND_TCB, but we only need it to
     * be able to create an epicsThreadId that is guaranteed never to be
     * the same as any current TID, and since TIDs are pointers this works.
     */

void epicsThreadOnce(epicsThreadOnceId *id, void (*func)(void *), void *arg)
{
    static struct epicsThreadOSD threadOnceComplete;
    #define EPICS_THREAD_ONCE_DONE &threadOnceComplete
    int result;

    epicsThreadInit();
    result = semTake(epicsThreadOnceMutex, WAIT_FOREVER);
    assert(result == OK);
    if (*id != EPICS_THREAD_ONCE_DONE) {
        if (*id == EPICS_THREAD_ONCE_INIT) { /* first call */
            *id = epicsThreadGetIdSelf();    /* mark active */
            semGive(epicsThreadOnceMutex);
            func(arg);
            result = semTake(epicsThreadOnceMutex, WAIT_FOREVER);
            assert(result == OK);
            *id = EPICS_THREAD_ONCE_DONE;    /* mark done */
        } else if (*id == epicsThreadGetIdSelf()) {
            semGive(epicsThreadOnceMutex);
            cantProceed("Recursive epicsThreadOnce() initialization\n");
        } else
            while (*id != EPICS_THREAD_ONCE_DONE) {
                /* Another thread is in the above func(arg) call. */
                semGive(epicsThreadOnceMutex);
                epicsThreadSleep(epicsThreadSleepQuantum());
                result = semTake(epicsThreadOnceMutex, WAIT_FOREVER);
                assert(result == OK);
            }
    }
    semGive(epicsThreadOnceMutex);
}

#ifdef EPICS_THREAD_CAN_JOIN

/* The next 2 routines are not static so they appear in the back-trace
 * of the epicsThreads that have called them.
 */
void epicsThreadAwaitingJoin(int tid)
{
    SEM_ID joinSem = (SEM_ID) taskSpareFieldGet(tid, joinField);
    STATUS status;

    if (!joinSem || (int) joinSem == ERROR)
        return;

    /* Wait for our supervisor */
    status = semTake(joinSem, JOIN_WARNING_TIMEOUT);
    if (status && errno == S_objLib_OBJ_TIMEOUT) {
        errlogPrintf("Warning: epicsThread '%s' still awaiting join\n",
            epicsThreadGetNameSelf());
        status = semTake(joinSem, WAIT_FOREVER);
    }
    if (status)
        perror("epicsThreadAwaitingJoin");

    taskSpareFieldSet(tid, joinField, 0);
    semDelete(joinSem);
}
  #define PREPARE_JOIN(tid, joinable) \
    taskSpareFieldSet(tid, joinField, \
        joinable ? (int) semBCreate(SEM_Q_FIFO, SEM_EMPTY) : 0)
  #define AWAIT_JOIN(tid) \
    epicsThreadAwaitingJoin(tid)

#else

  #define PREPARE_JOIN(tid, joinable)
  #define AWAIT_JOIN(tid)

#endif

void epicsThreadEntry(EPICSTHREADFUNC func, void *parm)
{
    int tid = taskIdSelf();

    taskVarAdd(tid,(int *)(char *)&papTSD);
    papTSD = NULL; /* Initialize for this thread */

    osdThreadHooksRun((epicsThreadId)tid);

    (*func)(parm);

    epicsExitCallAtThreadExits ();
    free(papTSD);
    taskVarDelete(tid,(int *)(char *)&papTSD);

    AWAIT_JOIN(tid);
}

#ifdef ALTIVEC
  #define TASK_FLAGS (VX_FP_TASK | VX_ALTIVEC_TASK)
#else
  #define TASK_FLAGS (VX_FP_TASK)
#endif
epicsThreadId epicsThreadCreateOpt(const char * name,
    EPICSTHREADFUNC funptr, void * parm, const epicsThreadOpts *opts )
{
    unsigned int stackSize;
    int tid;

    epicsThreadInit();

    if (!opts) {
        static const epicsThreadOpts opts_default = EPICS_THREAD_OPTS_INIT;
        opts = &opts_default;
    }
    stackSize = opts->stackSize;
    if (stackSize <= epicsThreadStackBig)
        stackSize = epicsThreadGetStackSize(stackSize);

    if (stackSize < 100) {
        errlogPrintf("epicsThreadCreate %s illegal stackSize %d\n",
            name, stackSize);
        return 0;
    }

    tid = taskCreate((char *)name,getOssPriorityValue(opts->priority),
        TASK_FLAGS, stackSize,
        (FUNCPTR)epicsThreadEntry, (int)funptr, (int)parm,
        0,0,0,0,0,0,0,0);
    if (tid == ERROR) {
        errlogPrintf("epicsThreadCreate %s failure %s\n",
            name, strerror(errno));
        return 0;
    }

    PREPARE_JOIN(tid, opts->joinable);
    taskActivate(tid);

    return (epicsThreadId)tid;
}

void epicsThreadMustJoin(epicsThreadId id)
{
#ifdef EPICS_THREAD_CAN_JOIN
    const char *fn = "epicsThreadMustJoin";
    int tid = (int) id;
    SEM_ID joinSem;
    STATUS status;

    if (!tid)
        return;

    joinSem = (SEM_ID) taskSpareFieldGet(tid, joinField);
    if ((int) joinSem == ERROR) {
        errlogPrintf("%s: Thread %#x no longer exists.\n", fn, tid);
        return;
    }

    if (tid == taskIdSelf()) {
        if (joinSem) {
            taskSpareFieldSet(tid, joinField, 0);
            semDelete(joinSem);
        }
        else {
            errlogPrintf("%s: Thread '%s' (%#x) can't self-join.\n",
                fn, epicsThreadGetNameSelf(), tid);
        }
        return;
    }

    if (!joinSem) {
        cantProceed("%s: Thread '%s' (%#x) is not joinable.\n",
            fn, taskName(tid), tid);
        return;
    }

    semGive(joinSem);   /* Rendezvous with thread */

    status = taskWait(tid, WAIT_FOREVER);
    if (status && errno != S_objLib_OBJ_ID_ERROR) {
        perror(fn);
        cantProceed("%s: ", fn);
    }
#endif
}

void epicsThreadSuspendSelf()
{
    STATUS status;

    status = taskSuspend(taskIdSelf());
    if(status) errlogPrintf("epicsThreadSuspendSelf failed\n");
}

void epicsThreadResume(epicsThreadId id)
{
    int tid = (int)id;
    STATUS status;

    status = taskResume(tid);
    if(status) errlogPrintf("epicsThreadResume failed\n");
}

void epicsThreadExitMain(void)
{
    errlogPrintf("epicsThreadExitMain was called for vxWorks. Why?\n");
}

unsigned int epicsThreadGetPriority(epicsThreadId id)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    status = taskPriorityGet(tid,&priority);
    if(status) errlogPrintf("epicsThreadGetPriority failed\n");
    return(getOsiPriorityValue(priority));
}

unsigned int epicsThreadGetPrioritySelf(void)
{
    return(epicsThreadGetPriority(epicsThreadGetIdSelf()));
}

void epicsThreadSetPriority(epicsThreadId id,unsigned int osip)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    priority = getOssPriorityValue(osip);
    status = taskPrioritySet(tid,priority);
    if(status) errlogPrintf("epicsThreadSetPriority failed\n");
}

epicsThreadBooleanStatus epicsThreadHighestPriorityLevelBelow(
    unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;
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

    newPriority = priority + 1;
    if (newPriority <= 99) {
        *pPriorityJustAbove = newPriority;
        return epicsThreadBooleanStatusSuccess;
    }
    return epicsThreadBooleanStatusFail;
}

int epicsThreadIsEqual(epicsThreadId id1, epicsThreadId id2)
{
    return((id1==id2) ? 1 : 0);
}

int epicsThreadIsSuspended(epicsThreadId id)
{
    int tid = (int)id;
    return((int)taskIsSuspended(tid));
}

void epicsThreadSleep(double seconds)
{
    STATUS status;
    int ticks;

    if (seconds > 0.0) {
        seconds *= sysClkRateGet();
        seconds += 0.99999999;  /* 8 9s here is optimal */
        ticks = (seconds >= INT_MAX) ? INT_MAX : (int) seconds;
    }
    else {  /* seconds <= 0 or NAN */
        ticks = 0;
    }
    status = taskDelay(ticks);
    if(status) errlogPrintf("epicsThreadSleep\n");
}

epicsThreadId epicsThreadGetIdSelf(void)
{
    return((epicsThreadId)taskIdSelf());
}

epicsThreadId epicsThreadGetId(const char *name)
{
    int tid = taskNameToId((char *)name);
    return((epicsThreadId)(tid==ERROR?0:tid));
}

const char *epicsThreadGetNameSelf(void)
{
    return taskName(taskIdSelf());
}

void epicsThreadGetName (epicsThreadId id, char *name, size_t size)
{
    int tid = (int)id;
    strncpy(name,taskName(tid),size-1);
    name[size-1] = '\0';
}

LIBCOM_API void epicsThreadMap ( EPICS_THREAD_HOOK_ROUTINE func )
{
    int noTasks = 0;
    int i;
    int result;

    result = semTake(epicsThreadListMutex, WAIT_FOREVER);
    assert(result == OK);
    while (noTasks == 0) {
        noTasks = taskIdListGet(taskIdList, taskIdListSize);
        if (noTasks == taskIdListSize) {
            taskIdList = realloc(taskIdList, (taskIdListSize+ID_LIST_CHUNK)*sizeof(int));
            assert(taskIdList);
            taskIdListSize += ID_LIST_CHUNK;
            noTasks = 0;
        }
    }
    for (i = 0; i < noTasks; i++) {
        func ((epicsThreadId)taskIdList[i]);
    }
    semGive(epicsThreadListMutex);
}

void epicsThreadShowAll(unsigned int level)
{
    taskShow(0,2);
}

void epicsThreadShow(epicsThreadId id, unsigned int level)
{
    int tid = (int)id;

    if (level > 1) level = 1;
    if (tid)
        taskShow(tid, level);
}

/* The following algorithm was thought of by Andrew Johnson APS/ASD .
 * The basic idea is to use a single vxWorks task variable.
 * The task variable is papTSD, which is an array of pointers to the TSD
 * The array size is equal to the number of epicsThreadPrivateIds created + 1
 * when epicsThreadPrivateSet is called.
 * Until the first call to epicsThreadPrivateCreate by a application papTSD=0
 * After first call papTSD[0] is value of nepicsThreadPrivate when
 * epicsThreadPrivateSet was last called by the thread. This is also
 * the value of epicsThreadPrivateId.
 * The algorithm allows for epicsThreadPrivateCreate being called after
 * the first call to epicsThreadPrivateSet.
 */
epicsThreadPrivateId epicsThreadPrivateCreate()
{
    static int lock = 0;
    epicsThreadPrivateId id;

    epicsThreadInit();
    /*lock is necessary because ++nepicsThreadPrivate may not be indivisible*/
    while(!vxTas(&lock)) taskDelay(1);
    id = (epicsThreadPrivateId)++nepicsThreadPrivate;
    lock = 0;
    return(id);
}

void epicsThreadPrivateDelete(epicsThreadPrivateId id)
{
    /*nothing to delete */
    return;
}

/*
 * No mutex is necessary here because by definition
 * thread variables are local to a single thread.
 */
void epicsThreadPrivateSet (epicsThreadPrivateId id, void *pvt)
{
    int int_id = (int)id;
    int nepicsThreadPrivateOld = 0;

    if (papTSD) nepicsThreadPrivateOld = (int)papTSD[0];

    if (!papTSD || nepicsThreadPrivateOld < int_id) {
        void **papTSDold = papTSD;
        int i;

        papTSD = callocMustSucceed(int_id + 1,sizeof(void *),
            "epicsThreadPrivateSet");
        papTSD[0] = (void *)(int_id);
        for (i = 1; i <= nepicsThreadPrivateOld; i++) {
            papTSD[i] = papTSDold[i];
        }
        free (papTSDold);
    }
    papTSD[int_id] = pvt;
}

void *epicsThreadPrivateGet(epicsThreadPrivateId id)
{
    int int_id = (int)id;

    /*
     * If epicsThreadPrivateGet() is called before epicsThreadPrivateSet()
     * for any particular thread, the value returned is always NULL.
     */
    if ( papTSD && int_id <= (int) papTSD[0] ) {
        return papTSD[int_id];
    }
    return NULL;
}

double epicsThreadSleepQuantum ()
{
    double HZ = sysClkRateGet ();
    return 1.0 / HZ;
}

LIBCOM_API int epicsThreadGetCPUs(void)
{
    return 1;
}
