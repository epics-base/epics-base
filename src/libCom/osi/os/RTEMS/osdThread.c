/*
 * RTEMS osdThread.c
 *      $Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
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
#include "ellLib.h"
#include "osiThread.h"
#include "cantProceed.h"

/*
 * Per-task variables
 */
struct taskVar {
    struct taskVar	*forw;
    struct taskVar	*back;
    char                *name;
    rtems_id		id;
    THREADFUNC          funptr;
    void                *parm;
    int                 threadVariableCapacity;
    void                **threadVariables;
    int			threadVariablesAdded;
};
static rtems_id taskVarMutex;
static struct taskVar *taskVarHead;
#define RTEMS_NOTEPAD_TASKVAR       11

/* Just map osi 0 to 99 into RTEMS 199 to 100 */
/* For RTEMS lower number means higher priority */
/* RTEMS = 100 + (99 - osi)  = 199 - osi */
/* osi =  199 - RTEMS */
int threadGetOsiPriorityValue(int ossPriority)
{
    return (199 - ossPriority);
}

int threadGetOssPriorityValue(int osiPriority)
{
    return (199 - osiPriority);
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
    rtems_status_code sc;

    if (!taskVarMutex) {
	rtems_mode mode;
	rtems_task_mode (RTEMS_NO_PREEMPT, RTEMS_PREEMPT_MASK, &mode);
	if (!taskVarMutex) {
	    sc = rtems_semaphore_create (rtems_build_name ('T', 'V', 'M', 'X'),
		1,
		RTEMS_FIFO | 
		    RTEMS_BINARY_SEMAPHORE |
		    RTEMS_NO_INHERIT_PRIORITY |
		    RTEMS_NO_PRIORITY_CEILING |
		    RTEMS_LOCAL,
		0,
		&taskVarMutex);
	    if (sc != RTEMS_SUCCESSFUL) {
		rtems_task_mode (mode, RTEMS_PREEMPT_MASK, &mode);
		syslog (LOG_ERR, "Can't create task variable mutex: %s", rtems_status_text (sc));
		cantProceed ("Can't create task variable mutex");
	    }
	}
	rtems_task_mode (mode, RTEMS_PREEMPT_MASK, &mode);
    }
    sc = rtems_semaphore_obtain (taskVarMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL) {
	syslog (LOG_ERR, "Can't obtain task variable mutex: %s", rtems_status_text (sc));
	cantProceed ("Can't obtain task variable mutex");
    }
}

static void
taskVarUnlock (void)
{
    rtems_status_code sc;

    sc = rtems_semaphore_release (taskVarMutex);
    if (sc != RTEMS_SUCCESSFUL) {
	syslog (LOG_ERR, "Can't release task variable mutex: %s", rtems_status_text (sc));
	cantProceed ("Can't release task variable mutex");
    }
}

/*
 * EPICS threads destroy themselves by returning from the thread entry function.
 * This simple wrapper provides the same semantics on RTEMS.
 */
static rtems_task
threadWrapper (rtems_task_argument arg)
{
    struct taskVar *v = (struct taskVar *)arg;

    taskVarLock ();
    v->forw = taskVarHead;
    v->back = NULL;
    if (v->forw)
	v->forw->back = v;
    taskVarHead = v;
    taskVarUnlock ();
    (*v->funptr)(v->parm);
    taskVarLock ();
    if (v->back)
	v->back->forw = v->forw;
    else
	taskVarHead = v->forw;
    if (v->forw)
	v->forw->back = v->back;
    taskVarUnlock ();
    free (v->name);
    free (v);
    rtems_task_delete (RTEMS_SELF);
}

/*
 * Create and start a new thread
 */
threadId
threadCreate (const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    struct taskVar *v;
    rtems_id tid;
    rtems_status_code sc;
    rtems_unsigned32 note;
    char c[4];

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
    v = mallocMustSucceed (sizeof *v, "threadCreate_vars");
    v->name = mallocMustSucceed (strlen (name) + 1, "threadCreate_name");
    strcpy (v->name, name);
    v->id = tid;
    v->funptr = funptr;
    v->parm = parm;
    v->threadVariableCapacity = 0;
    v->threadVariables = NULL;
    v->threadVariablesAdded = 0;
    note = (rtems_unsigned32)v;
    rtems_task_set_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, note);
    rtems_task_start (tid, threadWrapper, (rtems_task_argument)v);
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

void threadGetName(threadId id, char *name,size_t size)
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

/*
 * Thread private storage implementation based on the vxWorks
 * implementation by Andrew Johnson APS/ASD.
 */
static void *taskVarPointer = NULL;    /* RTEMS task-private variable */
static int threadVariableCount = 0;

threadPrivateId threadPrivateCreate ()
{
    return (void *)++threadVariableCount;
}

void threadPrivateDelete (threadPrivateId id)
{
    /* empty */
}

void threadPrivateSet (threadPrivateId id, void *pvt)
{
    int varIndex = (int)id;
    rtems_unsigned32 note;
    struct taskVar *v;

    /*
     * See if task variable has been set up
     * Task variables cost context switch time, so we don't add the
     * task variable until it's needed
     */
    rtems_task_get_note (RTEMS_SELF, RTEMS_NOTEPAD_TASKVAR, &note);
    v = (struct taskVar *)note;
    if (!v->threadVariablesAdded) {
        rtems_task_variable_add (RTEMS_SELF, &taskVarPointer, NULL);
	taskVarPointer = v;
	v->threadVariablesAdded = 1;
    }
    if (varIndex >= v->threadVariableCapacity) {
        v->threadVariables = realloc (v->threadVariables, (varIndex + 1) * sizeof (void *));
        if (v->threadVariables == NULL)
	    cantProceed("threadPrivateSet realloc failed\n");
	v->threadVariableCapacity = varIndex + 1;
    }
    v->threadVariables[varIndex] = pvt;
}

void * threadPrivateGet (threadPrivateId id)
{
    assert (taskVarPointer);
    return ((struct taskVar *)taskVarPointer)->threadVariables[(int)id];
}

/*
 * Show task info
 */
struct bitmap {
    char	    *msg;
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
	{ "RUN",   STATES_ALL_SET,		STATES_READY },
	{ "DORM",  STATES_DORMANT,		STATES_DORMANT },
	{ "SUSP",  STATES_SUSPENDED,		STATES_SUSPENDED },
	{ "TRANS", STATES_TRANSIENT,		STATES_TRANSIENT },
	{ "DELAY", STATES_DELAYING,		STATES_DELAYING },
	{ "Wtime", STATES_WAITING_FOR_TIME,	STATES_WAITING_FOR_TIME },
	{ "Wbuf",  STATES_WAITING_FOR_BUFFER,	STATES_WAITING_FOR_BUFFER },
	{ "Wseg",  STATES_WAITING_FOR_SEGMENT,	STATES_WAITING_FOR_SEGMENT },
	{ "Wmsg" , STATES_WAITING_FOR_MESSAGE,	STATES_WAITING_FOR_MESSAGE },
	{ "Wevnt", STATES_WAITING_FOR_EVENT,	STATES_WAITING_FOR_EVENT },
	{ "Wsem",  STATES_WAITING_FOR_SEMAPHORE,STATES_WAITING_FOR_SEMAPHORE },
	{ "Wmtx",  STATES_WAITING_FOR_MUTEX,	STATES_WAITING_FOR_MUTEX },
	{ "Wjoin", STATES_WAITING_FOR_JOIN_AT_EXIT,STATES_WAITING_FOR_JOIN_AT_EXIT },
	{ "Wrpc",  STATES_WAITING_FOR_RPC_REPLY,STATES_WAITING_FOR_RPC_REPLY },
	{ "Wrate", STATES_WAITING_FOR_PERIOD,	STATES_WAITING_FOR_PERIOD },
	{ "Wsig",  STATES_WAITING_FOR_SIGNAL,	STATES_WAITING_FOR_SIGNAL },
	{ NULL, 0, 0 },
    };

    the_thread = _Thread_Get (tid, &location);
    if (location != OBJECTS_LOCAL)
	return;
    thread = *the_thread;
    _Thread_Enable_dispatch();
    printf ("%4d", threadGetOsiPriorityValue(thread.current_priority));
    showBitmap (bitbuf, thread.current_state, taskState);
    printf ("%12.12s", bitbuf);
    if (thread.current_state & (STATES_WAITING_FOR_SEMAPHORE |
				STATES_WAITING_FOR_MUTEX |
				STATES_WAITING_FOR_MESSAGE))
	printf (" %8.8x", thread.Wait.id);
#endif
}

void threadShow (void)
{
    struct taskVar *v;

    printf ("   NAME        ID    PRI   STATE      WAIT\n");
    taskVarLock ();
    for (v = taskVarHead ; v != NULL ; v = v->forw) {
	printf ("%12.12s %8.8x", v->name, v->id);
	showInternalTaskInfo (v->id);
	printf ("\n");
    }
    taskVarUnlock ();
}
