/*
 * RTEMS osdSem.c
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

#include <assert.h>
#include <rtems.h>
#include <rtems/error.h>

#include "osiSem.h"
#include "errlog.h"

/*
 * Create a simple binary semaphore
 */
semBinaryId
semBinaryCreate(int initialState) 
{
    rtems_status_code sc;
    rtems_id sid;
    rtems_interrupt_level level;
    static char c1 = 'a';
    static char c2 = 'a';
    
    sc = rtems_semaphore_create (rtems_build_name ('S', 'B', c2, c1),
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
        if (c2 == 'z')
       	    c2 = 'a';
	else
            c2++;
	c1 = 'a';
    }
    else {
        c1++;
    }
    rtems_interrupt_enable (level);
    return (semBinaryId)sid;
}

semBinaryId semBinaryMustCreate(int initialState)
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
    
    sc = rtems_semaphore_obtain (sid, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
    if (sc == RTEMS_SUCCESSFUL)
        return semTakeOK;
    else if (sc == RTEMS_TIMEOUT)
        return semTakeTimeout;
    else
        return semTakeError;
}

void
semBinaryShow(semBinaryId id,int level)
{
}

semMutexId
semMutexCreate(void)
{
    rtems_status_code sc;
    rtems_id sid;
    rtems_interrupt_level level;
    static char c1 = 'a';
    static char c2 = 'a';
    
    sc = rtems_semaphore_create (rtems_build_name ('S', 'M', c2, c1),
        1,
        RTEMS_PRIORITY|RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY |RTEMS_NO_PRIORITY_CEILING|RTEMS_LOCAL,
        0,
        &sid);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create mutex semaphore: %s\n", rtems_status_text (sc));
        return NULL;
    }
    rtems_interrupt_disable (level);
    if (c1 == 'z') {
        if (c2 == 'z')
       	    c2 = 'a';
	else
            c2++;
	c1 = 'a';
    }
    else {
        c1++;
    }
    rtems_interrupt_enable (level);
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
	semBinaryDestroy (id);
}

void semMutexGive(semMutexId id)
{
	semBinaryGive (id);
}

semTakeStatus semMutexTake(semMutexId id)
{
	return semBinaryTake (id);
}

semTakeStatus semMutexTakeTimeout(
    semMutexId id, double timeOut)
{
	return semBinaryTakeTimeout (id, timeOut);
}

semTakeStatus semMutexTakeNoWait(semMutexId id)
{
	return semBinaryTakeNoWait (id);
}

epicsShareFunc void semMutexShow(semMutexId id,int level)
{
	semBinaryShow (id,level);
}
