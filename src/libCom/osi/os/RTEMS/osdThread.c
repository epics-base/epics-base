/*
 * RTEMS osiThread.c
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
#include <avl.h>

#include "errlog.h"
#include "ellLib.h"
#include "osiThread.h"
#include "cantProceed.h"

/*
 * This is one place where RTEMS needs a lot of help to implement
 * the EPICS operating-system-specific routines.
 * 1) RTEMS task names are at most 4 characters long.  This is solved
 *    by using a binary tree (AVL) to map long task names to RTEMS
 *    task I.D. numbers.
 * 2) RTEMS has no notion of task safe/unsafe states.  The solution
 *    here is to add a semaphore which must be held before a task
 *    can be deleted.
 * 3) RTEMS tasks do not destroy themselves when the entry function
 *    returns.  A simple wrapper function fixes this problem
 */

/*
 * Per-task variables
 */
struct taskVars {
    char                *name;
    THREADFUNC          funptr;
    void                *parm;
    rtems_id            taskSafeSemaphore;
    rtems_id            tid;
};

/*
 * RTEMS task notepad assignments
 */
#define TASK_VARIABLE_NOTE_INDEX     12

/*
 * Thread name lookup
 */
static rtems_id nameTableMutex;
static avl_tree *nameTable;

static int
compare_names (const void *a, const void *b, void *param)
{
    const struct taskVars *da = a, *db = b;
    int i;

    if ((i = db->name[0] - da->name[0]) == 0)
        return strcmp (da->name, db->name);
    return i;
}

void
rtems_epics_task_init (void)
{
    rtems_status_code sc;

    sc = rtems_semaphore_create (
          rtems_build_name ('T', 'H', 'S', 'H'),
          1,
          RTEMS_BINARY_SEMAPHORE | RTEMS_INHERIT_PRIORITY | RTEMS_PRIORITY,
          RTEMS_NO_PRIORITY,
          &nameTableMutex);
    if (sc !=  RTEMS_SUCCESSFUL)
        rtems_panic ("No nametable mutex");
    nameTable = avl_create (compare_names, NULL);
}

static rtems_id
tid_for_name (const char *name)
{
    struct taskVars entry, *e;
    rtems_id tid = 0;

    rtems_semaphore_obtain (nameTableMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    entry.name = (char *)name;
    e = avl_find (nameTable, &entry);
    if (e)
        tid = e->tid;
    rtems_semaphore_release (nameTableMutex);
    return tid;
}

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
 * The task wrapper
 */
static rtems_task
threadWrapper (rtems_task_argument arg)
{
    struct taskVars *v = (struct taskVars *)arg;

    (*v->funptr)(v->parm);
    threadDestroy (threadGetIdSelf ());
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
    rtems_id tid, safe;
    rtems_status_code sc;
    char c[4];
    static volatile int initialized;

    if (!initialized) {
            rtems_mode mode;

        rtems_task_mode (RTEMS_NO_PREEMPT, RTEMS_PREEMPT_MASK, &mode);
        if (!initialized) {
                rtems_epics_task_init ();
                initialized = 1;
        }
        rtems_task_mode (mode, RTEMS_PREEMPT_MASK, &mode);
    }

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
    sc = rtems_semaphore_create (rtems_build_name (c[0], c[1], c[2], c[3]),
         1,
         RTEMS_FIFO|RTEMS_BINARY_SEMAPHORE|RTEMS_NO_INHERIT_PRIORITY|RTEMS_NO_PRIORITY_CEILING|RTEMS_LOCAL,
         0,
         &safe);
    if (sc != RTEMS_SUCCESSFUL) {
        rtems_task_delete (tid);
        errlogPrintf("threadCreate create semaphore failure for %s: %s\n",name, rtems_status_text (sc));
        return 0;
    }

    v = mallocMustSucceed (sizeof *v, "threadCreate_vars");
    v->name = mallocMustSucceed (strlen (name) + 1, "threadCreate_name");
    strcpy (v->name, name);
    v->tid = tid;
    v->taskSafeSemaphore = safe;

    rtems_semaphore_obtain (nameTableMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    avl_insert (nameTable, v);
    rtems_semaphore_release (nameTableMutex);

    v->funptr = funptr;
    v->parm = parm;
    rtems_task_set_note (tid, TASK_VARIABLE_NOTE_INDEX, (rtems_unsigned32)v);
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

/*
 * Destroy a thread
 */
void
threadDestroy (threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_unsigned32 nv;
    struct taskVars *v;
    int deleted = 0;
    rtems_status_code sc;

    rtems_task_get_note (tid, TASK_VARIABLE_NOTE_INDEX, &nv);
    v = (struct taskVars *)nv;
    assert (tid == v->tid);
    rtems_semaphore_obtain (v->taskSafeSemaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    if ((tid != RTEMS_SELF) && (tid != (rtems_id)threadGetIdSelf())) {
        /* FIXME: Should probably obtain the network semaphore here! */
        rtems_task_delete (tid);
        deleted = 1;
    }
    rtems_semaphore_obtain (nameTableMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    avl_delete (nameTable, v);
    rtems_semaphore_release (nameTableMutex);
    rtems_semaphore_release (v->taskSafeSemaphore);
    rtems_semaphore_delete (v->taskSafeSemaphore);
    free (v->name);
    free (v);
    if (!deleted)
        sc = rtems_task_delete (tid);
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

int threadGetPriority(threadId id)
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
threadSetPriority (threadId id,int osip)
{
    rtems_id tid = (rtems_id)id;
    rtems_status_code sc;
    rtems_task_priority pri = threadGetOssPriorityValue(osip);

    sc = rtems_task_set_priority (tid, pri, &pri);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf("threadSetPriority failed\n", rtems_status_text (sc));
}

void
threadSetDestroySafe (void)
{
    rtems_unsigned32 nv;
    struct taskVars *v;

    rtems_task_get_note (RTEMS_SELF, TASK_VARIABLE_NOTE_INDEX, &nv);
    v = (struct taskVars *)nv;
    rtems_semaphore_obtain (v->taskSafeSemaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
}

void
threadSetDestroyUnsafe (void)
{
    rtems_unsigned32 nv;
    struct taskVars *v;

    rtems_task_get_note (RTEMS_SELF, TASK_VARIABLE_NOTE_INDEX, &nv);
    v = (struct taskVars *)nv;
    rtems_semaphore_release (v->taskSafeSemaphore);
}

const char *
threadGetName (threadId id)
{
    rtems_id tid = (rtems_id)id;
    rtems_unsigned32 nv;
    struct taskVars *v;

    rtems_task_get_note (tid, TASK_VARIABLE_NOTE_INDEX, &nv);
    v = (struct taskVars *)nv;
    return (v->name);
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

void
threadLockContextSwitch (void)
{
    rtems_mode oldMode;

    rtems_task_mode (RTEMS_NO_PREEMPT, RTEMS_PREEMPT_MASK, &oldMode);
}

void
threadUnlockContextSwitch (void)
{
    rtems_mode oldMode;

    rtems_task_mode (RTEMS_PREEMPT, RTEMS_PREEMPT_MASK, &oldMode);
}

threadId
threadNameToId (const char *name)
{
    return (threadId)tid_for_name (name);
}
