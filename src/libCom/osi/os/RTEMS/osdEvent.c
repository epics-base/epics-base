/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * RTEMS osdEvent.c
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

/*
 * We want to access information which is
 * normally hidden from application programs.
 */
#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#include <stdio.h>
#include <rtems.h>
#include <rtems/error.h>

#include "epicsEvent.h"
#include "epicsThread.h"
#include "errlog.h"

/* #define EPICS_RTEMS_SEMAPHORE_STATS */
/*
 * Some performance tuning instrumentation
 */
#ifdef EPICS_RTEMS_SEMAPHORE_STATS
unsigned long semEstat[4];
#define SEMSTAT(i)  semEstat[i]++;
#else
#define SEMSTAT(i) 
#endif

/*
 * Create a simple binary semaphore
 */
epicsEventId
epicsEventCreate(epicsEventInitialState initialState) 
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
    return (epicsEventId)sid;
}

void
epicsEventDestroy(epicsEventId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    sc = rtems_semaphore_delete (sid);
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't destroy semaphore: %s\n", rtems_status_text (sc));
}

epicsEventStatus
epicsEventTrigger(epicsEventId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    sc = rtems_semaphore_release (sid);
    if (sc == RTEMS_SUCCESSFUL)
        return epicsEventOK;
    errlogPrintf ("Can't release semaphore: %s\n", rtems_status_text (sc));
    return epicsEventError;
}

epicsEventStatus
epicsEventWait(epicsEventId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    SEMSTAT(0)
    sc = rtems_semaphore_obtain (sid, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
        return epicsEventError;
    return epicsEventOK;
}

epicsEventStatus
epicsEventWaitWithTimeout(epicsEventId id, double timeOut)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;
    
    if (timeOut <= 0.0)
        return epicsEventTryWait(id);
    SEMSTAT(1)
    delay = timeOut * rtemsTicksPerSecond_double;
    if (delay == 0)
        delay++;
    sc = rtems_semaphore_obtain (sid, RTEMS_WAIT, delay);
    if (sc == RTEMS_SUCCESSFUL)
        return epicsEventOK;
    else if (sc == RTEMS_TIMEOUT)
        return epicsEventWaitTimeout;
    else
        return epicsEventError;
}

epicsEventStatus
epicsEventTryWait(epicsEventId id)
{
    rtems_id sid = (rtems_id)id;
    rtems_status_code sc;
    
    SEMSTAT(2)
    sc = rtems_semaphore_obtain (sid, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
    if (sc == RTEMS_SUCCESSFUL)
        return epicsEventOK;
    else if (sc == RTEMS_UNSATISFIED)
        return epicsEventWaitTimeout;
    else
        return epicsEventError;
}

void
epicsEventShow(epicsEventId id, unsigned int level)
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
    printf ("          %8.8x  ", (int)sid);
    if (_Attributes_Is_counting_semaphore (semaphore.attribute_set)) {
            printf ("Count: %d", (int)semaphore.Core_control.semaphore.count);
    }
    else {
        if (_CORE_mutex_Is_locked(&semaphore.Core_control.mutex)) {
            char name[30];
            epicsThreadGetName ((epicsThreadId)semaphore.Core_control.mutex.holder_id, name, sizeof name);
            printf ("Held by:%8.8x (%s)  Nest count:%d",
                                    (unsigned int)semaphore.Core_control.mutex.holder_id,
                                    name,
                                    (int)semaphore.Core_control.mutex.nest_count);
        }
        else {
            printf ("Not Held");
        }
    }
    printf ("\n");
#endif
}
