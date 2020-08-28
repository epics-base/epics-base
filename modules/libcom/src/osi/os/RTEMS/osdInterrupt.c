/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * RTEMS osdInterrupt.c
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

#include <syslog.h>
#include <rtems.h>
#include <rtems/error.h>
#include "errlog.h"
#include "epicsInterrupt.h"
#include "epicsThread.h"

#define INTERRUPT_CONTEXT_MESSAGE_QUEUE_COUNT    100

static rtems_id interruptContextMessageQueue;

int
epicsInterruptLock (void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable (level);
    return level;
}

void
epicsInterruptUnlock (int key)
{
    rtems_interrupt_level level = key;

    rtems_interrupt_enable (level);
}

int
epicsInterruptIsInterruptContext (void)
{
	return rtems_interrupt_is_in_progress ();
}

/*
 * Pass a message from an interrupt context.
 * Note that this passes a pointer to the message, not the message itself.
 * This implies that the message must remain valid after the
 * interrupt context is no longer active.
 */
void
epicsInterruptContextMessage (const char *message)
{
    rtems_message_queue_send (interruptContextMessageQueue, &message, sizeof message);
}

/*
 * Daemon to process interrupt context messages
 */
void
InterruptContextMessageDaemon (void *unused)
{
    const char *message;
    size_t size;
    rtems_status_code sc;

    sc = rtems_message_queue_create (rtems_build_name ('I', 'C', 'M', 'Q'),
        INTERRUPT_CONTEXT_MESSAGE_QUEUE_COUNT,
        sizeof message,
        RTEMS_FIFO | RTEMS_LOCAL,
        &interruptContextMessageQueue);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create interrupt context message queue: %s\n", rtems_status_text (sc));
        epicsThreadSuspendSelf ();
    }
    for (;;) {
        sc = rtems_message_queue_receive (interruptContextMessageQueue,
            &message,
            &size,
            RTEMS_WAIT,
            RTEMS_NO_TIMEOUT);
        if (sc != RTEMS_SUCCESSFUL) {
            errlogPrintf ("Can't receive message from interrupt context: %s\n", rtems_status_text (sc));
	    epicsThreadSuspendSelf ();
        }
        if (size == sizeof message)
            syslog (LOG_ERR, "%s", message);
	else
            errlogPrintf ("Received %u-byte message from interrupt context", (unsigned int)size);
    }
}
