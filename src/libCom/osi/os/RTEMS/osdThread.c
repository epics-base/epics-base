/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * RTEMS osdThread.c
 *      Author: W. Eric Norum
 */

/*
 * We want to print out some task information which is
 * normally hidden from application programs.
 */
#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <syslog.h>
#include <limits.h>

#include <rtems.h>
#include <rtems/error.h>

#include "epicsStdio.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "osiUnistd.h"
#include "osdInterrupt.h"
#include "epicsExit.h"

epicsShareFunc void osdThreadHooksRun(epicsThreadId id);
epicsShareFunc void osdThreadHooksRunMain(epicsThreadId id);

/*
 * Per-task variables
 */
struct taskVar {
    struct taskVar  *forw;
    struct taskVar  *back;
    char            *name;
    rtems_id             id;
    EPICSTHREADFUNC              funptr;
    void                *parm;
    unsigned int        threadVariableCapacity;
    void                **threadVariables;
};
static struct epicsMutexOSD *taskVarMutex;
static struct taskVar *taskVarHead;
#define RTEMS_NOTEPAD_TASKVAR       11

/*
 * Support for `once-only' execution
 */
static volatile int initialized = 0; /* strictly speaking 'volatile' is not enough here, but it shouldn't hurt */
static struct epicsMutexOSD *onceMutex;

static
void epicsMutexOsdMustLock(struct epicsMutexOSD * L)
{
    while(epicsMutexOsdLock(L)!=epicsMutexLockOK) {
        cantProceed("epicsThreadOnce() mutex error");
    }
}

/*
 * Just map osi 0 to 99 into RTEMS 199 to 100
 * For RTEMS lower number means higher priority
 * RTEMS = 100 + (99 - osi)
  *      = 199 - osi
 *   osi =  199 - RTEMS
 */
int epicsThreadGetOsiPriorityValue(int ossPriority)
{
    if (ossPriority < 100) {
        return epicsThreadPriorityMax;
    }
    else if (ossPriority > 199) {
        return epicsThreadPriorityMin;
    }
    else {
        return (199u - (unsigned int)ossPriority);
    }
}

int epicsThreadGetOssPriorityValue(unsigned int osiPriority)
{
    if (osiPriority > 99) {
        return 100;
    }
    else {
        return (199 - (signed int)osiPriority);
    }
}

/*
 * epicsThreadLowestPriorityLevelAbove ()
 */
epicsShareFunc epicsThreadBooleanStatus epicsThreadLowestPriorityLevelAbove 
            (unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned newPriority = priority + 1;

    newPriority = priority + 1;
    if (newPriority <= 99) {
        *pPriorityJustAbove = newPriority;
        return epicsThreadBooleanStatusSuccess;
    }
    return epicsThreadBooleanStatusFail;
}

/*
 * epicsThreadHighestPriorityLevelBelow ()
 */
epicsShareFunc epicsThreadBooleanStatus epicsThreadHighestPriorityLevelBelow 
            (unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;

    if (newPriority <= 99) {
        *pPriorityJustBelow = newPriority;
        return epicsThreadBooleanStatusSuccess;
    }
    return epicsThreadBooleanStatusFail;
}

unsigned int
epicsThreadGetStackSize (epicsThreadStackSizeClass size)
{
    unsigned int stackSize = 11000;
    switch(size) {
    case epicsThreadStackSmall:  stackSize = 5000; break;
    case epicsThreadStackMedium: stackSize = 8000; break;
    case epicsThreadStackBig:                      break;
    default:
        errlogPrintf("epicsThreadGetStackSize illegal argument");
        break;
    }
    if (stackSize < RTEMS_MINIMUM_STACK_SIZE)
        stackSize = RTEMS_MINIMUM_STACK_SIZE;
    return stackSize;
}

/*
 * Ensure integrity of task variable list
 */
static void
taskVarLock (void)
{
    epicsMutexOsdMustLock (taskVarMutex);
}

static void
taskVarUnlock (void)
{
    epicsMutexOsdUnlock (taskVarMutex);
}

/*
 * EPICS threads destroy themselves by returning from the thread entry function.
 * This simple wrapper provides the same semantics on RTEMS.
 */
static rtems_task
threadWrapper (rtems_task_argument arg)
{
    struct taskVar *v = (struct taskVar *)arg;

    osdThreadHooksRun((epicsThreadId)v->id);
    (*v->funptr)(v->parm);
    epicsExitCallAtThreadExits ();
    taskVarLock ();
    if (v->back)
        v->back->forw = v->forw;
    else
        taskVarHead = v->forw;
    if (v->forw)
        v->forw->back = v->back;
    taskVarUnlock ();
    free (v->threadVariables);
    free (v->name);
    free (v);
    rtems_task_delete (RTEMS_SELF);
}

/*
 * The task wrapper takes care of cleanup
 */
void epicsThreadExitMain (void)
{
}

static void
setThreadInfo(rtems_id tid, const char *name, EPICSTHREADFUNC funptr,
    void *parm)
{
    struct taskVar *v;
    uint32_t note;
    rtems_status_code sc;

    v = mallocMustSucceed (sizeof *v, "epicsThreadCreate_vars");
    v->name = epicsStrDup(name);
    v->id = tid;
    v->funptr = funptr;
    v->parm = parm;
    v->threadVariableCapacity = 0;
    v->threadVariables = NULL;
    note = (uint32_t)v;
    rtems_task_set_note (tid, RTEMS_NOTEPAD_TASKVAR, note);
    taskVarLock ();
    v->forw = taskVarHead;
    v->back = NULL;
    if (v->forw)
        v->forw->back = v;
    taskVarHead = v;
    taskVarUnlock ();
    if (funptr) {
        sc = rtems_task_start (tid, threadWrapper, (rtems_task_argument)v);
        if (sc !=  RTEMS_SUCCESSFUL)
            errlogPrintf ("setThreadInfo:  Can't start  %s: %s\n",
                name, rtems_status_text(sc));
    }
}

/*
 * OS-dependent initialization
 * No need to worry about making this thread-safe since
 * it must be called before epicsThreadCreate creates
 * any new threads.
 */
static void
epicsThreadInit (void)
{
    if (!initialized) {
        rtems_id tid;
        rtems_task_priority old;

        rtems_task_set_priority (RTEMS_SELF, epicsThreadGetOssPriorityValue(99), &old);
        onceMutex = epicsMutexOsdCreate();
        taskVarMutex = epicsMutexOsdCreate();
        if (!onceMutex || !taskVarMutex)
            cantProceed("epicsThreadInit() can't create global mutexes\n");
        rtems_task_ident (RTEMS_SELF, 0, &tid);
        setThreadInfo (tid, "_main_", NULL, NULL);
        osdThreadHooksRunMain((epicsThreadId)tid);
        initialized = 1;
        epicsThreadCreate ("ImsgDaemon", 99,
                epicsThreadGetStackSize (epicsThreadStackSmall),
                InterruptContextMessageDaemon, NULL);
    }
}

void epicsThreadRealtimeLock(void)
{}

/*
 * Create and start a new thread
 */
epicsThreadId
epicsThreadCreate (const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm)
{
    rtems_id tid;
    rtems_status_code sc;
    char c[4];

    if (!initialized) epicsThreadInit();
    if (stackSize < RTEMS_MINIMUM_STACK_SIZE) {
        errlogPrintf ("Warning: epicsThreadCreate %s illegal stackSize %d\n",
            name, stackSize);
        stackSize = RTEMS_MINIMUM_STACK_SIZE;
    }
    strncpy (c, name, sizeof c);
    sc = rtems_task_create (rtems_build_name (c[0], c[1], c[2], c[3]),
         epicsThreadGetOssPriorityValue (priority),
         stackSize,
         RTEMS_PREEMPT|RTEMS_NO_TIMESLICE|RTEMS_NO_ASR|RTEMS_INTERRUPT_LEVEL(0),
         RTEMS_FLOATING_POINT|RTEMS_LOCAL,
         &tid);
    if (sc !=  RTEMS_SUCCESSFUL) {
        errlogPrintf ("epicsThreadCreate create failure for %s: %s\n",
            name, rtems_status_text(sc));
        return 0;
    }
    setThreadInfo (tid, name, funptr,parm);
    return (epicsThreadId)tid;
}

epicsThreadId
threadMustCreate (const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm)
{
    epicsThreadId tid = epicsThreadCreate(name, priority, stackSize, funptr, parm);

    if (tid == NULL)
        cantProceed(0);
    return tid;
}

void
epicsThreadSuspendSelf (void)
{
    rtems_status_code sc;

    sc = rtems_task_suspend (RTEMS_SELF);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("epicsThreadSuspendSelf failed: %s\n",
            rtems_status_text(sc));
}

void epicsThreadResume(epicsThreadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;

    sc = rtems_task_resume (tid);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("epicsThreadResume failed: %s\n",
            rtems_status_text (sc));
}

unsigned int epicsThreadGetPriority(epicsThreadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_task_priority pri;

    sc = rtems_task_set_priority (tid, RTEMS_CURRENT_PRIORITY, &pri);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("epicsThreadGetPriority failed: %s\n",
            rtems_status_text(sc));
    return epicsThreadGetOsiPriorityValue(pri);
}

unsigned int epicsThreadGetPrioritySelf(void)
{
    return epicsThreadGetPriority((epicsThreadId)RTEMS_SELF);
}

void
epicsThreadSetPriority (epicsThreadId id,unsigned int osip)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_task_priority pri = epicsThreadGetOssPriorityValue(osip);

    sc = rtems_task_set_priority (tid, pri, &pri);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("epicsThreadSetPriority failed: %s\n",
            rtems_status_text(sc));
}

int
epicsThreadIsEqual (epicsThreadId id1, epicsThreadId id2)
{
    return (id1 == id2);
}

int
epicsThreadIsSuspended (epicsThreadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;

    switch (sc = rtems_task_is_suspended (tid)) {
    case RTEMS_SUCCESSFUL:
        return 0;

    case RTEMS_ALREADY_SUSPENDED:
        return 1;

    default:
        return 1;
    }
}

void
epicsThreadSleep (double seconds)
{
    rtems_status_code sc;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;

    if (seconds > 0.0) {
        seconds *= rtemsTicksPerSecond_double;
        seconds += 0.99999999;  /* 8 9s here is optimal */
        delay = (seconds >= INT_MAX) ? INT_MAX : (int) seconds;
    }
    else {  /* seconds <= 0 or NAN */
        delay = 0;
    }
    sc = rtems_task_wake_after (delay);
    if(sc != RTEMS_SUCCESSFUL)
        errlogPrintf("epicsThreadSleep: %s\n", rtems_status_text(sc));
}

epicsThreadId
epicsThreadGetIdSelf (void)
{
    rtems_id tid;

    rtems_task_ident (RTEMS_SELF, 0, &tid);
    return (epicsThreadId)tid;
}

const char *epicsThreadGetNameSelf(void)
{
    uint32_t note;
    struct taskVar *v;

    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (void *)note;
    return v->name;
}

void epicsThreadGetName (epicsThreadId id, char *name, size_t size)
{
    rtems_id tid = (rtems_id)id;
    struct taskVar *v;
    int haveName = 0;

    taskVarLock ();
    for (v=taskVarHead ; v != NULL ; v=v->forw) {
        if (v->id == tid) {
            strncpy(name, v->name, size);
            haveName = 1;
            break;
        }
    }
    taskVarUnlock ();
    if (!haveName) {
#if (__RTEMS_MAJOR__>4 || \
    (__RTEMS_MAJOR__==4 && __RTEMS_MINOR__>8) || \
    (__RTEMS_MAJOR__==4 && __RTEMS_MINOR__==8 && __RTEMS_REVISION__>=99))
        if (_Objects_Get_name_as_string((rtems_id)id, size, name) != NULL)
            haveName = 1;
#else
        /*
         * Try to get the RTEMS task name
         */
        Thread_Control *thr;
        Objects_Locations l;
        if ((thr=_Thread_Get(tid, &l)) != NULL) {
            if (OBJECTS_LOCAL == l) {
                int length;
                Objects_Information *oi = _Objects_Get_information(tid);
                if (oi->name_length >= size)
                    length = size - 1;
                else
                    length = oi->name_length;
                 if (oi->is_string)
                     strncpy(name, thr->Object.name, length);
                else
                    _Objects_Copy_name_raw( &thr->Object.name, name, length);
                name[length] = '\0';
                haveName = 1;
            }
            _Thread_Enable_dispatch();
        }
#endif
    }
    if (!haveName)
        snprintf(name, size, "0x%lx", (long)tid);
    name[size-1] = '\0';
}

epicsThreadId epicsThreadGetId (const char *name)
{
    struct taskVar *v;
    rtems_id tid = 0;

    /*
     * Linear search is good enough since this routine
     * is invoked only by command-line calls.
     */
    taskVarLock ();
    for (v = taskVarHead ; v != NULL ; v = v->forw) {
        if (strcmp (name, v->name) == 0) {
            tid = v->id;
            break;
        } 
    }
    taskVarUnlock ();
    return (epicsThreadId)tid;
}

/*
 * Ensure func() is run only once.
 */
void epicsThreadOnce(epicsThreadOnceId *id, void(*func)(void *), void *arg)
{
    #define EPICS_THREAD_ONCE_DONE (epicsThreadId) 1

    if (!initialized) epicsThreadInit();
    epicsMutexOsdMustLock(onceMutex);
    if (*id != EPICS_THREAD_ONCE_DONE) {
        if (*id == EPICS_THREAD_ONCE_INIT) { /* first call */
            *id = epicsThreadGetIdSelf();    /* mark active */
            epicsMutexOsdUnlock(onceMutex);
            func(arg);
            epicsMutexOsdMustLock(onceMutex);
            *id = EPICS_THREAD_ONCE_DONE;    /* mark done */
        } else if (*id == epicsThreadGetIdSelf()) {
            epicsMutexOsdUnlock(onceMutex);
            cantProceed("Recursive epicsThreadOnce() initialization\n");
        } else
            while (*id != EPICS_THREAD_ONCE_DONE) {
                /* Another thread is in the above func(arg) call. */
                epicsMutexOsdUnlock(onceMutex);
                epicsThreadSleep(epicsThreadSleepQuantum());
                epicsMutexOsdMustLock(onceMutex);
            }
    }
    epicsMutexOsdUnlock(onceMutex);
}

/*
 * Thread private storage implementation based on the vxWorks
 * implementation by Andrew Johnson APS/ASD.
 */
epicsThreadPrivateId epicsThreadPrivateCreate ()
{
    unsigned int taskVarIndex;
    static volatile unsigned int threadVariableCount = 0;

    if (!initialized) epicsThreadInit ();
    taskVarLock ();
    taskVarIndex = ++threadVariableCount;
    taskVarUnlock ();
    return (epicsThreadPrivateId)taskVarIndex;
}

void epicsThreadPrivateDelete (epicsThreadPrivateId id)
{
    /* empty */
}

void epicsThreadPrivateSet (epicsThreadPrivateId id, void *pvt)
{
    unsigned int varIndex = (unsigned int)id;
    uint32_t note;
    struct taskVar *v;
    int i;

    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (struct taskVar *)note;
    if (varIndex >= v->threadVariableCapacity) {
        v->threadVariables = realloc (v->threadVariables,
            (varIndex + 1) * sizeof(void *));
        if (v->threadVariables == NULL)
            cantProceed("epicsThreadPrivateSet realloc failed\n");
        for (i = v->threadVariableCapacity ; i < varIndex ; i++)
            v->threadVariables[i] = NULL;
        v->threadVariableCapacity = varIndex + 1;
    }
    v->threadVariables[varIndex] = pvt;
}

void * epicsThreadPrivateGet (epicsThreadPrivateId id)
{
    unsigned int varIndex = (unsigned int)id;
    uint32_t note;
    struct taskVar *v;

    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (struct taskVar *)note;
    if (varIndex >= v->threadVariableCapacity)
        return NULL;
    return v->threadVariables[varIndex];
}

/*
 * Show task info
 */
struct bitmap {
    char            *msg;
    unsigned long   mask;
    unsigned long   state;
};

static void
showBitmap (char *cbuf, unsigned long bits, const struct bitmap *bp)
{
    for ( ; bp->msg != NULL ; bp++) {
        if ((bp->mask & bits) == bp->state) {
            strcpy (cbuf, bp->msg);
            cbuf += strlen (bp->msg);
        }
    }
}

static void
showInternalTaskInfo (rtems_id tid)
{
#ifdef __RTEMS_VIOLATE_KERNEL_VISIBILITY__
    int epicsPri;
    Thread_Control *the_thread;
    Objects_Locations location;
    static Thread_Control thread;
    static char bitbuf[120];
    static const struct bitmap taskState[] = {
        { "RUN",   STATES_ALL_SET,              STATES_READY },
        { "DORM",  STATES_DORMANT,              STATES_DORMANT },
        { "SUSP",  STATES_SUSPENDED,            STATES_SUSPENDED },
        { "TRANS", STATES_TRANSIENT,            STATES_TRANSIENT },
        { "Delay", STATES_DELAYING,             STATES_DELAYING },
        { "Wtime", STATES_WAITING_FOR_TIME,     STATES_WAITING_FOR_TIME },
        { "Wbuf",  STATES_WAITING_FOR_BUFFER,   STATES_WAITING_FOR_BUFFER },
        { "Wseg",  STATES_WAITING_FOR_SEGMENT,  STATES_WAITING_FOR_SEGMENT },
        { "Wmsg" , STATES_WAITING_FOR_MESSAGE,  STATES_WAITING_FOR_MESSAGE },
        { "Wevnt", STATES_WAITING_FOR_EVENT,    STATES_WAITING_FOR_EVENT },
        { "Wsem",  STATES_WAITING_FOR_SEMAPHORE,STATES_WAITING_FOR_SEMAPHORE },
        { "Wmtx",  STATES_WAITING_FOR_MUTEX,    STATES_WAITING_FOR_MUTEX },
        { "Wjoin", STATES_WAITING_FOR_JOIN_AT_EXIT,STATES_WAITING_FOR_JOIN_AT_EXIT },
        { "Wrpc",  STATES_WAITING_FOR_RPC_REPLY,STATES_WAITING_FOR_RPC_REPLY },
        { "Wrate", STATES_WAITING_FOR_PERIOD,   STATES_WAITING_FOR_PERIOD },
        { "Wsig",  STATES_WAITING_FOR_SIGNAL,   STATES_WAITING_FOR_SIGNAL },
        { NULL, 0, 0 },
    };

    the_thread = _Thread_Get (tid, &location);
    if (location != OBJECTS_LOCAL) {
        fprintf(epicsGetStdout(),"%-30s",  "  *** RTEMS task gone! ***");
        return;
    }
    thread = *the_thread;
    _Thread_Enable_dispatch();
    /*
     * Show both real and current priorities if they differ.
     * Note that the epicsThreadGetOsiPriorityValue routine is not used here.
     * If a thread has a priority outside the normal EPICS range then
     * that priority should be displayed, not the value truncated to
     * the EPICS range.
     */
    epicsPri = 199-thread.real_priority;
    if (epicsPri < 0)
        fprintf(epicsGetStdout(),"   <0");
    else if (epicsPri > 99)
        fprintf(epicsGetStdout(),"  >99");
    else
        fprintf(epicsGetStdout()," %4d", epicsPri);
    if (thread.current_priority == thread.real_priority)
        fprintf(epicsGetStdout(),"%4d    ", (int)thread.current_priority);
    else
        fprintf(epicsGetStdout(),"%4d/%-3d", (int)thread.real_priority, (int)thread.current_priority);
    showBitmap (bitbuf, thread.current_state, taskState);
    fprintf(epicsGetStdout(),"%8.8s", bitbuf);
    if (thread.current_state & (STATES_WAITING_FOR_SEMAPHORE |
                                STATES_WAITING_FOR_MUTEX |
                                STATES_WAITING_FOR_MESSAGE))
        fprintf(epicsGetStdout()," %8.8x", (int)thread.Wait.id);
    else
        fprintf(epicsGetStdout()," %8.8s", "");
#endif
}

static void
epicsThreadShowInfo (struct taskVar *v, unsigned int level)
{
    if (!v) {
        fprintf(epicsGetStdout(),"            PRIORITY\n");
        fprintf(epicsGetStdout(),"    ID    EPICS RTEMS   STATE    WAIT         NAME\n");
        fprintf(epicsGetStdout(),"+--------+-----------+--------+--------+---------------------+\n");
    } else {
        fprintf(epicsGetStdout(),"%9.8x", (int)v->id);
        showInternalTaskInfo (v->id);
        fprintf(epicsGetStdout()," %s\n", v->name);
    }
}

void epicsThreadShow (epicsThreadId id, unsigned int level)
{
    struct taskVar *v;

    if (!id) {
        epicsThreadShowInfo (NULL, level);
        return;
    }
    taskVarLock ();
    for (v = taskVarHead ; v != NULL ; v = v->forw) {
        if ((rtems_id)id == v->id) {
            epicsThreadShowInfo (v, level);
            taskVarUnlock ();
            return;
        }
    }
    taskVarUnlock ();
    fprintf(epicsGetStdout(),"*** Thread %x does not exist.\n", (unsigned int)id);
}

void epicsThreadMap(EPICS_THREAD_HOOK_ROUTINE func)
{
    struct taskVar *v;

    taskVarLock ();
    /*
     * Map tasks in the order of creation (backwards through list)
     */
    for (v = taskVarHead ; v != NULL && v->forw != NULL ; v = v->forw)
        continue;
    while (v) {
        func ((epicsThreadId)v->id);
        v = v->back;
    }
    taskVarUnlock ();
}

void epicsThreadShowAll (unsigned int level)
{
    struct taskVar *v;

    epicsThreadShowInfo (NULL, level);
    taskVarLock ();
    /*
     * Show tasks in the order of creation (backwards through list)
     */
    for (v = taskVarHead ; v != NULL && v->forw != NULL ; v = v->forw)
        continue;
    while (v) {
        epicsThreadShowInfo (v, level);
        v = v->back;
    }
    taskVarUnlock ();
}

double epicsThreadSleepQuantum ( void )
{
    extern double rtemsTicksPerSecond_double;

    return 1.0 / rtemsTicksPerSecond_double;
}

epicsShareFunc int epicsThreadGetCPUs(void)
{
#if defined(RTEMS_SMP)
    return rtems_smp_get_number_of_processors();
#else
    return 1;
#endif
}
