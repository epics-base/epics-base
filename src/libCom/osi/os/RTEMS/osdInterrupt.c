/*
 * RTEMS osiInterrupt.c
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

#include <syslog.h>
#include <rtems.h>
#include <rtems/error.h>
#include "errlog.h"

#define INTERRUPT_CONTEXT_MESSAGE_QUEUE_COUNT    100

static rtems_id interruptContextMessageQueue;

int
interruptLock (void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable (level);
    return level;
}

void
interruptUnlock (int key)
{
    rtems_interrupt_level level = key;

    rtems_interrupt_enable (level);
}

int
interruptIsInterruptContext (void)
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
interruptContextMessage (const char *message)
{
    rtems_message_queue_send (interruptContextMessageQueue, &message, sizeof message);
}

/*
 * Daemon to process interrupt context messages
 */
rtems_task
InterruptContextMessageDaemon (rtems_task_argument arg)
{
    const char *message;
    rtems_unsigned32 size;
    rtems_status_code sc;

    sc = rtems_message_queue_create (rtems_build_name ('I', 'C', 'M', 'Q'),
        INTERRUPT_CONTEXT_MESSAGE_QUEUE_COUNT,
        sizeof message,
        RTEMS_FIFO | RTEMS_LOCAL,
        &interruptContextMessageQueue);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf ("Can't create interrupt context message queue: %s\n", rtems_status_text (sc));
        rtems_task_suspend (RTEMS_SELF);
    }
    for (;;) {
        sc = rtems_message_queue_receive (interruptContextMessageQueue,
            &message,
            &size,
            RTEMS_WAIT,
            RTEMS_NO_TIMEOUT);
        if (sc != RTEMS_SUCCESSFUL) {
            errlogPrintf ("Can't receive message from interrupt context: %s\n", rtems_status_text (sc));
            rtems_task_suspend (RTEMS_SELF);
        }
        if (size == sizeof message)
            syslog (LOG_ERR, "%s", message);
	else
            errlogPrintf ("Received %d-byte message from interrupt context", size);
    }
}
