/*
 * RTEMS osdMutex.c
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

#include "epicsMutex.h"
#include "epicsEvent.h"
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

epicsMutexId
epicsMutexCreate(void)
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
      Semaphore_Control *the_semaphore;
      Objects_Locations  location;

      the_semaphore = _Semaphore_Get( sid, &location );
      _Thread_Enable_dispatch();  

      return the_semaphore;
    }
#endif
    return (epicsMutexId)sid;
}

epicsMutexId epicsMutexMustCreate(void)
{
    epicsMutexId id = epicsMutexCreate ();
    assert (id);
    return id;
}

void epicsMutexDestroy(epicsMutexId id)
{
    rtems_status_code sc;
    rtems_id sid;
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    sid = the_semaphore->Object.id;
#else
    sid = (rtems_id)id;
#endif
    sc = rtems_semaphore_delete (sid);
    if (sc == RTEMS_RESOURCE_IN_USE) {
        rtems_semaphore_release (sid);
        sc = rtems_semaphore_delete (sid);
    }
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't destroy semaphore: %s\n", rtems_status_text (sc));
}

void epicsMutexUnlock(epicsMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    _Thread_Disable_dispatch();
	_CORE_mutex_Surrender (
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        NULL
        );
    _Thread_Enable_dispatch();
#else
    epicsEventSignal (id);
#endif

}

epicsMutexLockStatus epicsMutexLock(epicsMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    ISR_Level level;
    SEMSTAT(3)
    _ISR_Disable( level );
    _CORE_mutex_Seize(
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        1,   /* TRUE or FALSE */
        0, /* same as passed to obtain -- ticks */
        level
    );
    if (_Thread_Executing->Wait.return_code == 0)
        return epicsMutexLockOK;
    else
        return epicsMutexLockError;
#else
    SEMSTAT(3)
    return((epicsEventWait (id) == epicsEventWaitOK)
        ?epicsMutexLockOK : epicsMutexLockError);
#endif
}

epicsMutexLockStatus epicsMutexLockWithTimeout(
    epicsMutexId id, double timeOut)
{
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    ISR_Level level;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;

    SEMSTAT(4)
    delay = timeOut * rtemsTicksPerSecond_double;
    if (delay == 0)
        delay = 1;
    _ISR_Disable( level );
    _CORE_mutex_Seize(
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        1,   /* TRUE or FALSE */
        delay, /* same as passed to obtain -- ticks */
        level
    );
    if (_Thread_Executing->Wait.return_code == 0)
        return epicsMutexLockOK;
    else
        return epicsMutexLockError;
#else
    epicsEventWaitStatus status;
    SEMSTAT(4)
    status = epicsEventWaitWithTimeout(id,timeOut);
    return((status==epicsEventWaitOK
        ? epicsMutexLockOK
        : (status==epicsEventWaitTimeout)
           ? epicsMutexLockTimeout
           : epicsMutexLockError));
#endif
}

epicsMutexLockStatus epicsMutexTryLock(epicsMutexId id)
{
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    ISR_Level level;
    SEMSTAT(5)
    _ISR_Disable( level );
    _CORE_mutex_Seize(
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        0,   /* TRUE or FALSE */
        0, /* same as passed to obtain -- ticks */
        level
    );
    if (_Thread_Executing->Wait.return_code == 0)
        return epicsMutexLockOK;
    else
        return epicsMutexLockError;
#else
    epicsEventWaitStatus status;
    SEMSTAT(5)
    status = epicsEventTryWait(id);
    return((status==epicsEventWaitOK
        ? epicsMutexLockOK
        : (status==epicsEventWaitTimeout)
           ? epicsMutexLockTimeout
           : epicsMutexLockError));
#endif
}

epicsShareFunc void epicsMutexShow(epicsMutexId id,unsigned int level)
{
#ifdef RTEMS_FAST_MUTEX
    Semaphore_Control *the_semaphore = (Semaphore_Control *)id;
    id = (epicsMutexId)the_semaphore->Object.id;
#endif
	epicsEventShow (id,level);
}
