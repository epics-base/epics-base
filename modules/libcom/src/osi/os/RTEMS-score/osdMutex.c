/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * RTEMS osdMutex.c
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

/*
 * We want to access information which is
 * normally hidden from application programs.
 */
#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <rtems.h>
#include <rtems/error.h>

#define EPICS_PRIVATE_API

#include "epicsStdio.h"
#include "epicsMutex.h"
#include "epicsMutexImpl.h"
#include "rtemsNamePvt.h"
#include "epicsEvent.h"
#include "errlog.h"

uint32_t next_rtems_name(char prefix, uint32_t* counter)
{
    uint32_t next;
    rtems_interrupt_level level;
    char a, b, c;

    rtems_interrupt_disable (level);
    next = *counter;
    *counter = (next+1)%(26u*26u*26u);
    rtems_interrupt_enable (level);

    a = 'a' + (next % 26u);
    next /= 26u;
    b = 'a' + (next % 26u);
    next /= 26u;
    c = 'a' + (next % 26u); // modulo should be redundant, but ... paranoia

    return rtems_build_name(prefix, a, b, c);
}

void epicsMutexOsdSetup(void)
{
    // TODO: use RTEMS_SYSINIT_ITEM() ?
    if(!epicsMutexGlobalLock.osd) {
        epicsMutexOsdPrepare(&epicsMutexGlobalLock);
    }
}

long epicsMutexOsdPrepare(struct epicsMutexParm *mutex)
{
    rtems_status_code sc;
    rtems_id sid;
    rtems_interrupt_level level;
    static uint32_t name;

    sc = rtems_semaphore_create (next_rtems_name ('M', &name),
        1,
        RTEMS_PRIORITY|RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY|RTEMS_NO_PRIORITY_CEILING|RTEMS_LOCAL,
        0,
        &sid);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create mutex semaphore: %s\n", rtems_status_text (sc));
        return ENOMEM;
    }
    {
        Objects_Locations  location;

        mutex->osd = _Semaphore_Get( sid, &location );
        _Thread_Enable_dispatch(); /* _Semaphore_Get() disables */

        return 0;
    }
}

void epicsMutexOsdCleanup(struct epicsMutexParm *mutex)
{
    rtems_status_code sc;
    rtems_id sid;
    Semaphore_Control *the_semaphore = mutex->osd;
    sid = the_semaphore->Object.id;
    sc = rtems_semaphore_delete (sid);
    if (sc == RTEMS_RESOURCE_IN_USE) {
        rtems_semaphore_release (sid);
        sc = rtems_semaphore_delete (sid);
    }
    if (sc != RTEMS_SUCCESSFUL)
        errlogPrintf ("Can't destroy semaphore %p (%lx): %s\n",
                      mutex, (unsigned long)sid, rtems_status_text (sc));
}

void epicsMutexUnlock(struct epicsMutexParm *mutex)
{
    Semaphore_Control *the_semaphore = mutex->osd;
    _Thread_Disable_dispatch();
    _CORE_mutex_Surrender (
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        NULL
        );
    _Thread_Enable_dispatch();

}

epicsMutexLockStatus epicsMutexLock(struct epicsMutexParm *mutex)
{
    Semaphore_Control *the_semaphore = mutex->osd;
    ISR_Level level;
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
}

epicsMutexLockStatus epicsMutexTryLock(struct epicsMutexParm *mutex)
{
    Semaphore_Control *the_semaphore = mutex->osd;
    ISR_Level level;
    _ISR_Disable( level );
    _CORE_mutex_Seize(
        &the_semaphore->Core_control.mutex,
        the_semaphore->Object.id,
        0,   /* TRUE or FALSE */
        0, /* same as passed to obtain -- ticks */
        level
    );
    if (_Thread_Executing->Wait.return_code == CORE_MUTEX_STATUS_SUCCESSFUL)
        return epicsMutexLockOK;
    else if (_Thread_Executing->Wait.return_code == CORE_MUTEX_STATUS_UNSATISFIED_NOWAIT)
        return epicsMutexLockTimeout;
    else
        return epicsMutexLockError;
}

LIBCOM_API void epicsMutexOsdShow(struct epicsMutexParm *mutex,unsigned int level)
{
    Semaphore_Control *the_semaphore = mutex->osd;
    epicsEventShow ((epicsEventId)the_semaphore->Object.id,level);
}

void epicsMutexOsdShowAll(void) {}
