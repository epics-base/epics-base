/*
 * RTEMS osdThread.c
 *      $Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
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

#include <rtems.h>
#include <rtems/error.h>

#include "errlog.h"
#include "epicsMutex.h"
#include "osiThread.h"
#include "osiInterrupt.h"
#include "cantProceed.h"

/*
 * Per-task variables
 */
struct taskVar {
    struct taskVar  *forw;
    struct taskVar  *back;
    char            *name;
    rtems_id             id;
    THREADFUNC              funptr;
    void                *parm;
    unsigned int        threadVariableCapacity;
    void                **threadVariables;
};
static epicsMutexId taskVarMutex;
static struct taskVar *taskVarHead;
#define RTEMS_NOTEPAD_TASKVAR       11

/*
 * Support for `once-only' execution
 */
static int initialized;
static epicsMutexId onceMutex;

/*
 * Just map osi 0 to 99 into RTEMS 199 to 100
 * For RTEMS lower number means higher priority
 * RTEMS = 100 + (99 - osi)
  *      = 199 - osi
 *   osi =  199 - RTEMS
 */
int threadGetOsiPriorityValue(int ossPriority)
{
    if (ossPriority < 100) {
        return threadPriorityMax;
    }
    else if (ossPriority > 199) {
        return threadPriorityMin;
    }
    else {
        return (199u - (unsigned int)ossPriority);
    }
}

int threadGetOssPriorityValue(unsigned int osiPriority)
{
    if (osiPriority > 99) {
        return 100;
    }
    else {
        return (199 - (signed int)osiPriority);
    }
}

/*
 * threadLowestPriorityLevelAbove ()
 */
epicsShareFunc threadBoolStatus epicsShareAPI threadLowestPriorityLevelAbove 
            (unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned newPriority = priority + 1;

    newPriority = priority + 1;
    if (newPriority <= 99) {
        *pPriorityJustAbove = newPriority;
        return tbsSuccess;
    }
    return tbsFail;
}

/*
 * threadHighestPriorityLevelBelow ()
 */
epicsShareFunc threadBoolStatus epicsShareAPI threadHighestPriorityLevelBelow 
            (unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;

    if (newPriority <= 99) {
        *pPriorityJustBelow = newPriority;
        return tbsSuccess;
    }
    return tbsFail;
}

#define ARCH_STACK_FACTOR 2

unsigned int
threadGetStackSize (threadStackSizeClass size)
{
    switch(size) {
    case threadStackSmall:  return( 4000*ARCH_STACK_FACTOR);
    case threadStackMedium: return( 6000*ARCH_STACK_FACTOR);
    case threadStackBig:    return(11000*ARCH_STACK_FACTOR);
    default:
        errlogPrintf("threadGetStackSize illegal argument");
    }
    return(11000*ARCH_STACK_FACTOR);
}

/*
 * Ensure integrity of task variable list
 */
static void
taskVarLock (void)
{
    epicsMutexLock (taskVarMutex);
}

static void
taskVarUnlock (void)
{
    epicsMutexUnlock (taskVarMutex);
}

/*
 * EPICS threads destroy themselves by returning from the thread entry function.
 * This simple wrapper provides the same semantics on RTEMS.
 */
static rtems_task
threadWrapper (rtems_task_argument arg)
{
    struct taskVar *v = (struct taskVar *)arg;

    (*v->funptr)(v->parm);
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
void threadExitMain (void)
{
}

/*
 * Report initialization failures
 */
static void
badInit (const char *msg)
{
    const char fmt[] = "%s called before threadInit finished!";

    syslog (LOG_CRIT, fmt, msg);
    fprintf (stderr, fmt, msg);
    fprintf (stderr, "\n");
    rtems_task_suspend (RTEMS_SELF);
}

static void
setThreadInfo (rtems_id tid, const char *name, THREADFUNC funptr,void *parm)
{
    struct taskVar *v;
    rtems_unsigned32 note;

    v = mallocMustSucceed (sizeof *v, "threadCreate_vars");
    v->name = mallocMustSucceed (strlen (name) + 1, "threadCreate_name");
    strcpy (v->name, name);
    v->id = tid;
    v->funptr = funptr;
    v->parm = parm;
    v->threadVariableCapacity = 0;
    v->threadVariables = NULL;
    note = (rtems_unsigned32)v;
    rtems_task_set_note (tid, RTEMS_NOTEPAD_TASKVAR, note);
    taskVarLock ();
    v->forw = taskVarHead;
    v->back = NULL;
    if (v->forw)
        v->forw->back = v;
    taskVarHead = v;
    taskVarUnlock ();
    if (funptr)
        rtems_task_start (tid, threadWrapper, (rtems_task_argument)v);
}

/*
 * OS-dependent initialization
 * No need to worry about making this thread-safe since
 * it must be called before threadCreate creates
 * any new threads.
 */
void
threadInit (void)
{
    if (!initialized) {
        rtems_id tid;
        rtems_task_priority old;
        extern void clockInit (void);

        clockInit ();
        rtems_task_set_priority (RTEMS_SELF, threadGetOssPriorityValue(99), &old);
        onceMutex = epicsMutexMustCreate();
        taskVarMutex = epicsMutexMustCreate ();
        rtems_task_ident (RTEMS_SELF, 0, &tid);
        setThreadInfo (tid, "_main_", NULL, NULL);
        initialized = 1;
        threadCreate ("ImsgDaemon", 99,
                threadGetStackSize (threadStackSmall),
                InterruptContextMessageDaemon, NULL);
    }
}

/*
 * Create and start a new thread
 */
threadId
threadCreate (const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    rtems_id tid;
    rtems_status_code sc;
    char c[4];

    if (!initialized) threadInit();
    if (stackSize < RTEMS_MINIMUM_STACK_SIZE) {
        errlogPrintf ("threadCreate %s illegal stackSize %d\n",name,stackSize);
        return 0;
    }
    strncpy (c, name, sizeof c);
    sc = rtems_task_create (rtems_build_name (c[0], c[1], c[2], c[3]),
         threadGetOssPriorityValue (priority),
         stackSize,
         RTEMS_PREEMPT|RTEMS_NO_TIMESLICE|RTEMS_NO_ASR|RTEMS_INTERRUPT_LEVEL(0),
         RTEMS_FLOATING_POINT|RTEMS_LOCAL,
         &tid);
    if (sc !=  RTEMS_SUCCESSFUL) {
        errlogPrintf ("threadCreate create failure for %s: %s\n",name, rtems_status_text (sc));
        return 0;
    }
    setThreadInfo (tid, name, funptr,parm);
    return (threadId)tid;
}

threadId
threadMustCreate (const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    threadId tid;

    if ((tid = threadCreate (name, priority, stackSize, funptr, parm)) == NULL)
        cantProceed (0);
    return tid;
}

void
threadSuspendSelf (void)
{
    rtems_status_code sc;

    sc = rtems_task_suspend (RTEMS_SELF);
    if(sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadSuspendSelf failed: %s\n", rtems_status_text (sc));
}

void threadResume(threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;

    sc = rtems_task_resume (tid);
    if(sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadResume failed: %s\n", rtems_status_text (sc));
}

unsigned int threadGetPriority(threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_task_priority pri;

    sc = rtems_task_set_priority (tid, RTEMS_CURRENT_PRIORITY, &pri);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadGetPriority failed: %s\n", rtems_status_text (sc));
    return threadGetOsiPriorityValue (pri);
}

unsigned int threadGetPrioritySelf(void)
{
    return threadGetPriority((threadId)RTEMS_SELF);
}

void
threadSetPriority (threadId id,unsigned int osip)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_task_priority pri = threadGetOssPriorityValue(osip);

    sc = rtems_task_set_priority (tid, pri, &pri);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadSetPriority failed\n", rtems_status_text (sc));
}

int
threadIsEqual (threadId id1, threadId id2)
{
    return (id1 == id2);
}

int
threadIsSuspended (threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;

    switch (sc = rtems_task_is_suspended (tid)) {
    case RTEMS_SUCCESSFUL:
        return 0;

    case RTEMS_ALREADY_SUSPENDED:
        return 1;

    default:
        errlogPrintf("threadIsSuspended: %s\n", rtems_status_text (sc));
        return 0;
    }
}

void
threadSleep (double seconds)
{
    rtems_status_code sc;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;
    
    delay = seconds * rtemsTicksPerSecond_double;
    sc = rtems_task_wake_after (delay);
    if(sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadSleep: %s\n", rtems_status_text (sc));
}

threadId
threadGetIdSelf (void)
{
    rtems_id tid;

    rtems_task_ident (RTEMS_SELF, 0, &tid);
    return (threadId)tid;
}

const char *threadGetNameSelf(void)
{
    rtems_unsigned32 note;
    struct taskVar *v;

    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (void *)note;
    return v->name;
}

void threadGetName (threadId id, char *name, size_t size)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_unsigned32 note;
    struct taskVar *v;

    taskVarLock ();
    sc = rtems_task_get_note (tid, RTEMS_NOTEPAD_TASKVAR, &note);
    if (sc == RTEMS_SUCCESSFUL) {
        v = (void *)note;
        strncpy (name, v->name, size - 1);
        name[size-1] = '\0';
    }
    else {
        *name = '\0';
    }
    taskVarUnlock ();
}

threadId threadGetId (const char *name)
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
    return (threadId)tid;
}

/*
 * Ensure func() is run only once.
 */
void threadOnceOsd(threadOnceId *id, void(*func)(void *), void *arg)
{
    if (!initialized) threadInit();
    epicsMutexMustLock(onceMutex);
    if (*id == 0) {
            *id = -1;
            func(arg);
            *id = 1;
    }
    epicsMutexUnlock(onceMutex);
}

/*
 * Thread private storage implementation based on the vxWorks
 * implementation by Andrew Johnson APS/ASD.
 */
threadPrivateId threadPrivateCreate ()
{
    unsigned int taskVarIndex;
    static volatile unsigned int threadVariableCount = 0;

    if (!initialized) threadInit ();
    taskVarLock ();
    taskVarIndex = ++threadVariableCount;
    taskVarUnlock ();
    return (threadPrivateId)taskVarIndex;
}

void threadPrivateDelete (threadPrivateId id)
{
    /* empty */
}

void threadPrivateSet (threadPrivateId id, void *pvt)
{
    unsigned int varIndex = (unsigned int)id;
    rtems_unsigned32 note;
    struct taskVar *v;
    int i;

    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (struct taskVar *)note;
    if (varIndex >= v->threadVariableCapacity) {
        v->threadVariables = realloc (v->threadVariables, (varIndex + 1) * sizeof (void *));
        if (v->threadVariables == NULL)
            cantProceed("threadPrivateSet realloc failed\n");
        for (i = v->threadVariableCapacity ; i < varIndex ; i++)
            v->threadVariables[i] = NULL;
        v->threadVariableCapacity = varIndex + 1;
    }
    v->threadVariables[varIndex] = pvt;
}

void * threadPrivateGet (threadPrivateId id)
{
    unsigned int varIndex = (unsigned int)id;
    rtems_unsigned32 note;
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
    Thread_Control *the_thread;
    Objects_Locations location;
    static Thread_Control thread;
    static char bitbuf[120];
    static const struct bitmap taskState[] = {
        { "RUN",   STATES_ALL_SET,              STATES_READY },
        { "DORM",  STATES_DORMANT,              STATES_DORMANT },
        { "SUSP",  STATES_SUSPENDED,            STATES_SUSPENDED },
        { "TRANS", STATES_TRANSIENT,            STATES_TRANSIENT },
        { "DELAY", STATES_DELAYING,             STATES_DELAYING },
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
    if (location != OBJECTS_LOCAL)
        return;
    thread = *the_thread;
    _Thread_Enable_dispatch();
    /*
     * Show both real and current priorities if they differ.
     * Note that the threadGetOsiPriorityValue routine is not used here.
     * If a thread has a priority outside the normal EPICS range then
     * that priority should be displayed, not the value truncated to
     * the EPICS range.
     */
    if (thread.current_priority == thread.real_priority)
        printf ("%4d     ", 199-thread.current_priority);
    else
        printf ("%4d/%-4d", 199-thread.current_priority, 199-thread.real_priority);
    showBitmap (bitbuf, thread.current_state, taskState);
    printf ("%9.9s", bitbuf);
    if (thread.current_state & (STATES_WAITING_FOR_SEMAPHORE |
                                STATES_WAITING_FOR_MUTEX |
                                STATES_WAITING_FOR_MESSAGE))
        printf (" %8.8x", thread.Wait.id);
#endif
}

static void
threadShowHeader (void)
{
    printf ("      NAME        ID    PRIORITY   STATE    WAIT   \n");
    printf ("+-------------+--------+--------+--------+--------+\n");
}

static void
threadShowInfo (struct taskVar *v, unsigned int level)
{
        printf ("%14.14s %8.8x", v->name, v->id);
        showInternalTaskInfo (v->id);
        printf ("\n");
}

void threadShow (threadId id, unsigned int level)
{
    struct taskVar *v;

    if (!id) {
        threadShowHeader ();
        return;
    }
    taskVarLock ();
    for (v = taskVarHead ; v != NULL ; v = v->forw) {
        if ((rtems_id)id == v->id) {
            threadShowInfo (v, level);
            return;
        }
    }
    taskVarUnlock ();
    printf ("*** Thread %x does not exist.\n", (unsigned int)id);
}

void threadShowAll (unsigned int level)
{
    struct taskVar *v;

    threadShowHeader ();
    taskVarLock ();
    /*
     * Show tasks in the order of creation (backwards through list)
     */
    for (v = taskVarHead ; v != NULL && v->forw != NULL ; v = v->forw)
        continue;
    while (v) {
        threadShowInfo (v, level);
        v = v->back;
    }
    taskVarUnlock ();
}
