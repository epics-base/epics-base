/*
 * RTEMS osdSem.c
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

/*
 * We want to print out some information which is
 * normally hidden from application programs.
 */
#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#include <assert.h>
#include <stdio.h>
#include <rtems.h>
#include <rtems/error.h>

#include "osiSem.h"
#include "osiThread.h"
#include "errlog.h"

#define RTEMS_FAST_MUTEX
/* #define EPICS_RTEMS_SEMAPHORE_STATS */
/*
 * Some performance tuning instrumentation
 */
#ifdef EPICS_RTEMS_SEMAPHORE_STATS
unsigned long semStat[6];
#define SEMSTAT(i)  semStat[i]++;
#else
#define SEMSTAT(i) 
#endif

#ifdef RTEMS_FAST_MUTEX
struct semMutex {
    rtems_id            id;
    Semaphore_Control   *the_semaphore;
};
#endif

/*
 * Create a simple binary semaphore
 */
semBinaryId
semBinaryCreate(semInitialState initialState) 
{
    rtems_status_code sc;
    rtems_id sid;
    rtems_interrupt_level level;
    static char c1 = 'a';
    static char c2 = 'a';
    static char c3 = 'a';
    
    sc = rtems_semaphore_create (rtems_build_name ('B', c3, c2, c1),
        initialState,
	RTEMS_FIFO | RTEMS_SIMPLE_BINARY_SEMAPHORE |
	    RTEMS_NO_INHERIT_PRIORITY | RTEMS_NO_PRIORITY_CEILING | RTEMS_LOCAL,
        0,
        &sid);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create binary semaphore: %s\n", rtems_status_text (sc));
        return NULL;
    }
    rtems_interrupt_disable (level);
    if (c1 == 'z') {
        if (c2 == 'z') {
            if (c3 == 'z') {
                c3 = 'a';
            }
            else {
                c3++;
            }
            c2 = 'a';
        }
        else {
            c2++;
        }
        c1 = 'a';
    }
    else {
        c1++;
    }
    rtems_interrupt_enable (level);
    return (semBinaryId)sid;
}

semBinaryId semBinaryMustCreate(semInitialState initialState)
{
    semBinaryId id = semBinaryCreate (initialState);
    assert (id);
    return id;
}

void
semBinaryDestroy(semBinaryId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    sc = rtems_semaphore_delete (sid);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't destroy semaphore: %s\n", rtems_status_text (sc));
}

void
semBinaryGive(semBinaryId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    sc = rtems_semaphore_release (sid);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't release semaphore: %s\n", rtems_status_text (sc));
}

semTakeStatus
semBinaryTake(semBinaryId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    SEMSTAT(0)
    sc = rtems_semaphore_obtain (sid, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
        return semTakeError;
    return semTakeOK;
}

semTakeStatus
semBinaryTakeTimeout(semBinaryId id, double timeOut)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;
    
    SEMSTAT(1)
    delay = timeOut * rtemsTicksPerSecond_double;
    if (delay == 0)
        delay = 1;
    sc = rtems_semaphore_obtain (sid, RTEMS_WAIT, delay);
    if (sc == RTEMS_SUCCESSFUL)
        return semTakeOK;
    else if (sc == RTEMS_TIMEOUT)
        return semTakeTimeout;
    else
        return semTakeError;
}

semTakeStatus
semBinaryTakeNoWait(semBinaryId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    SEMSTAT(2)
    sc = rtems_semaphore_obtain (sid, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
    if (sc == RTEMS_SUCCESSFUL)
        return semTakeOK;
    else if (sc == RTEMS_UNSATISFIED)
        return semTakeTimeout;
    else
        return semTakeError;
}

void
semBinaryShow(semBinaryId id, unsigned int level)
{
#if __RTEMS_VIOLATE_KERNEL_VISIBILITY__
    rtems_id sid = (rtems_id)id;
    Semaphore_Control *the_semaphore;
    Semaphore_Control semaphore;
    Objects_Locations location;

    the_semaphore = _Semaphore_Get (sid, &location);
    if (location != OBJECTS_LOCAL)
        return;
    /*
     * Yes, there's a race condition here since an interrupt might
     * change things while the copy is in progress, but the information
     * is only for display, so it's not that critical.
     */
    semaphore = *the_semaphore;
    _Thread_Enable_dispatch();
    printf ("          %8.8x  ", sid);
    if (_Attributes_Is_counting_semaphore (semaphore.attribute_set)) {
            printf ("Count: %d", semaphore.Core_control.semaphore.count);
    }
    else {
        if (_CORE_mutex_Is_locked(&semaphore.Core_control.mutex)) {
            char name[20];
            threadGetName ((threadId)semaphore.Core_control.mutex.holder_id, name, sizeof name);
            printf ("Held by:%8.8x (%s)  Nest count:%d",
                                    semaphore.Core_control.mutex.holder_id,
                                    name,
                                    semaphore.Core_control.mutex.nest_count);
        }
        else {
            printf ("Not Held");
        }
    }
    printf ("\n");
#endif
}

semMutexId
semMutexCreate(void)
{
    rtems_status_code sc;
    rtems_id sid;
    rtems_interrupt_level level;
    static char c1 = 'a';
    static char c2 = 'a';
    static char c3 = 'a';
    
    sc = rtems_semaphore_create (rtems_build_name ('M', c3, c2, c1),
        1,
        RTEMS_PRIORITY|RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY|RTEMS_NO_PRIORITY_CEILING|RTEMS_LOCAL,
        0,
        &sid);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create mutex semaphore: %s\n", rtems_status_text (sc));
        return NULL;
    }
    rtems_interrupt_disable (level);
    if (c1 == 'z') {
        if (c2 == 'z') {
            if (c3 == 'z') {
                c3 = 'a';
            }
            else {
                c3++;
            }
            c2 = 'a';
        }
        else {
            c2++;
        }
        c1 = 'a';
    }
    else {
        c1++;
    }
    rtems_interrupt_enable (level);
#ifdef RTEMS_FAST_MUTEX
    {
      struct semMutex *smid = mallocMustSucceed(sizeof *smid, "semMutexCreate");
      register Semaphore_Control *the_semaphore;
      Objects_Locations           location;

      the_semaphore = _Semaphore_Get( sid, &location );
      _Thread_Enable_dispatch();  

      smid->id = sid;
      smid->the_semaphore = the_semaphore;
      return smid;
    }
#endif
    return (semMutexId)sid;
}

semMutexId semMutexMustCreate(void)
{
    semMutexId id = semMutexCreate ();
    assert (id);
    return id;
}

void semMutexDestroy(semMutexId id)
{
    rtems_status_code sc;
#ifdef RTEMS_FAST_MUTEX
    struct semMutex *sid = (struct semMutex *)id;

    sc = rtems_semaphore_delete (sid->id);
#else
    rtems_id sid = (rtems_id)id;

    sc = rtems_semaphore_delete (sid);
#endif
    
    if (sc == RTEMS_RESOURCE_IN_USE) {
#ifdef RTEMS_FAST_MUTEX
        rtems_semaphore_release (sid->id);
        sc = rtems_semaphore_delete (sid->id);
#else
        rtems_semaphore_release (sid);
        sc = rtems_semaphore_delete (sid);
#endif
    }
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't destroy semaphore: %s\n", rtems_status_text (sc));
#ifdef RTEMS_FAST_MUTEX
    free (sid);
#endif
}

void semMutexGive(semMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    struct semMutex *sid = (struct semMutex *)id;
    _Thread_Disable_dispatch();
	_CORE_mutex_Surrender (
        &sid->the_semaphore->Core_control.mutex,
        sid->id,
        NULL
        );
    _Thread_Enable_dispatch();
#else
    semBinaryGive (id);
#endif

}

semTakeStatus semMutexTake(semMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    struct semMutex *sid = (struct semMutex *)id;
    SEMSTAT(3)
    _Thread_Disable_dispatch();
    _CORE_mutex_Seize(
        &sid->the_semaphore->Core_control.mutex,
        sid->id,
        1,   /* TRUE or FALSE */
        0 /* same as passed to obtain -- ticks */
    );
    _Thread_Enable_dispatch();
    if (_Thread_Executing->Wait.return_code == 0)
        return semTakeOK;
    else
        return semTakeError;
#else
    SEMSTAT(3)
    return semBinaryTake (id);
#endif
}

semTakeStatus semMutexTakeTimeout(
    semMutexId id, double timeOut)
{
#ifdef RTEMS_FAST_MUTEX
    struct semMutex *sid = (struct semMutex *)id;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;

    SEMSTAT(4)
    delay = timeOut * rtemsTicksPerSecond_double;
    if (delay == 0)
        delay = 1;
    _Thread_Disable_dispatch();
    _CORE_mutex_Seize(
        &sid->the_semaphore->Core_control.mutex,
        sid->id,
        1,   /* TRUE or FALSE */
        delay /* same as passed to obtain -- ticks */
    );
    _Thread_Enable_dispatch();
    if (_Thread_Executing->Wait.return_code == 0)
        return semTakeOK;
    else
        return semTakeTimeout;
#else
    SEMSTAT(4)
    return semBinaryTakeTimeout (id, timeOut);
#endif
}

semTakeStatus semMutexTakeNoWait(semMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    struct semMutex *sid = (struct semMutex *)id;
    SEMSTAT(5)
    _Thread_Disable_dispatch();
    _CORE_mutex_Seize(
        &sid->the_semaphore->Core_control.mutex,
        sid->id,
        0,   /* TRUE or FALSE */
        0 /* same as passed to obtain -- ticks */
    );
    _Thread_Enable_dispatch();
    if (_Thread_Executing->Wait.return_code == 0)
        return semTakeOK;
    else
        return semTakeTimeout;
#else
    SEMSTAT(5)
    return semBinaryTakeNoWait (id);
#endif
}

epicsShareFunc void semMutexShow(semMutexId id,unsigned int level)
{
	semBinaryShow (id,level);
}
