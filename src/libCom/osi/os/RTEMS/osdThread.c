/*
 * RTEMS osdThread.c
 *      $Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#include <rtems.h>
#include <rtems/error.h>

#include "errlog.h"
#include "ellLib.h"
#include "osiThread.h"
#include "cantProceed.h"

/*
 * Per-task variables
 */
struct taskVars {
    char                *name;
    THREADFUNC          funptr;
    void                *parm;
};

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
 * EPICS threads destroy themselves by returning from the thread entry function.
 * This simple wrapper provides the same semantics on RTEMS.
 */
static rtems_task
threadWrapper (rtems_task_argument arg)
{
    struct taskVars *v = (struct taskVars *)arg;

    (*v->funptr)(v->parm);
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
    struct taskVars *v;
    rtems_id tid;
    rtems_status_code sc;
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
    v->funptr = funptr;
    v->parm = parm;
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
threadSuspend (threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;

    sc = rtems_task_suspend (tid);
    if(sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadSuspend failed: %s\n", rtems_status_text (sc));
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

int
threadIsReady (threadId id)
{
    return !threadIsSuspended(id);
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
