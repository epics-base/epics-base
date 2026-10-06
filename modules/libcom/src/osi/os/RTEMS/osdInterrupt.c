/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
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

#if defined(RTEMS_SMP)
#include <rtems/sysinit.h>
#include <epicsSpin.h>
#endif /* RTEMS_SMP */

#define INTERRUPT_CONTEXT_MESSAGE_QUEUE_COUNT    100

static rtems_id interruptContextMessageQueue;

/*
 * SMP does not support interrupt disable and enable because
 * interrupts are local to a processor. RTEMS provides locks to use
 * with SMP however they do not fit the existing EPICS API as there is
 * no way to hold and return a lock context.
 *
 * Use an EPICS spinlock and create it before the POSIX init thread
 * runs.
 */
#if defined(RTEMS_SMP)
static epicsSpinId isr_lock;
static void create_isr_lock(void) {
    isr_lock = epicsSpinMustCreate();
}
RTEMS_SYSINIT_ITEM(
    create_isr_lock, RTEMS_SYSINIT_POSIX_USER_THREADS, RTEMS_SYSINIT_ORDER_FIRST);
#endif /* RTEMS_SMP */

int
epicsInterruptLock (void)
{
#if defined(RTEMS_SMP)
    epicsSpinLock (isr_lock);
    return 0;
#else  /* RTEMS_SMP */
    rtems_interrupt_level level;
    rtems_interrupt_disable (level);
    return level;
#endif /* RTEMS_SMP */
}

void
epicsInterruptUnlock (int key)
{
#if defined(RTEMS_SMP)
    (void) key;
    epicsSpinUnlock (isr_lock);
#else /* RTEMS_SMP */
    rtems_interrupt_level level = key;
    rtems_interrupt_enable (level);
#endif /* RTEMS_SMP */
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
