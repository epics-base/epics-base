/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

/*
 * We want to access information which is
 * normally hidden from application programs.
 */
#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#define epicsExportSharedSymbols
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtems.h>
#include <rtems/error.h>
#include "epicsMessageQueue.h"
#include "errlog.h"

epicsShareFunc epicsMessageQueueId epicsShareAPI
epicsMessageQueueCreate(unsigned int capacity, unsigned int maximumMessageSize)
{
    rtems_status_code sc;
    epicsMessageQueueId id = calloc(1, sizeof(*id));
    rtems_interrupt_level level;
    static char c1 = 'a';
    static char c2 = 'a';
    static char c3 = 'a';

    if(!id)
        return NULL;
    
    sc = rtems_message_queue_create (rtems_build_name ('Q', c3, c2, c1),
        capacity,
        maximumMessageSize,
        RTEMS_FIFO|RTEMS_LOCAL,
        &id->id);
    if (sc != RTEMS_SUCCESSFUL) {
        free(id);
        errlogPrintf ("Can't create message queue: %s\n", rtems_status_text (sc));
        return NULL;
    }
    id->maxSize = maximumMessageSize;
    id->localBuf = NULL;
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
    return id;
}

static rtems_status_code rtems_message_queue_send_timeout(
    rtems_id id,
    void *buffer,
    uint32_t size,
    rtems_interval timeout)
{
  Message_queue_Control    *the_message_queue;
  Objects_Locations         location;
  CORE_message_queue_Status msg_status;
    
  the_message_queue = _Message_queue_Get( id, &location );
  switch ( location )
  {
    case OBJECTS_ERROR:
      return RTEMS_INVALID_ID;

    case OBJECTS_LOCAL:
      msg_status = _CORE_message_queue_Send(
        &the_message_queue->message_queue,
        buffer,
        size,
        id,
        NULL,
        1,
        timeout
      );

      _Thread_Enable_dispatch();

      /*
       *  If we had to block, then this is where the task returns
       *  after it wakes up.  The returned status is correct for
       *  non-blocking operations but if we blocked, then we need 
       *  to look at the status in our TCB.
       */

      if ( msg_status == CORE_MESSAGE_QUEUE_STATUS_UNSATISFIED_WAIT )
        msg_status = _Thread_Executing->Wait.return_code;
      return _Message_queue_Translate_core_message_queue_return_code( msg_status );
  }
  return RTEMS_INTERNAL_ERROR;   /* unreached - only to remove warnings */
}

epicsShareFunc int epicsShareAPI epicsMessageQueueSend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize)
{
    if (rtems_message_queue_send_timeout(id->id, message, messageSize, RTEMS_NO_TIMEOUT) == RTEMS_SUCCESSFUL)
        return 0;
    else
        return -1;
}

epicsShareFunc int epicsShareAPI epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize,
    double timeout)
{
    rtems_interval delay;
    extern double rtemsTicksPerSecond_double;
    
    /*
     * Convert time to ticks
     */
    if (timeout <= 0.0)
        return epicsMessageQueueTrySend(id, message, messageSize);
    delay = (int)(timeout * rtemsTicksPerSecond_double);
    if (delay == 0)
        delay++;
    if (rtems_message_queue_send_timeout(id->id, message, messageSize, delay) == RTEMS_SUCCESSFUL)
        return 0;
    else
        return -1;
}

static int receiveMessage(
    epicsMessageQueueId id,
    void *buffer,
    uint32_t size,
    uint32_t wait,
    rtems_interval delay)
{
    size_t rsize;
    rtems_status_code sc;
    
    if (size < id->maxSize) {
        if (id->localBuf == NULL) {
            id->localBuf = malloc(id->maxSize);
            if (id->localBuf == NULL)
                return -1;
        }
        rsize = receiveMessage(id, id->localBuf, id->maxSize, wait, delay);
        if (rsize > size)
            return -1;
        memcpy(buffer, id->localBuf, rsize);
    }
    else {
        sc = rtems_message_queue_receive(id->id, buffer, &rsize, wait, delay);
        if (sc != RTEMS_SUCCESSFUL)
            return -1;
    }
    return rsize;
}

epicsShareFunc int epicsShareAPI epicsMessageQueueTryReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size)
{
    return receiveMessage(id, message, size, RTEMS_NO_WAIT, 0);
}

epicsShareFunc int epicsShareAPI epicsMessageQueueReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size)
{
    return receiveMessage(id, message, size, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
}

epicsShareFunc int epicsShareAPI epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout)
{
    rtems_interval delay;
    uint32_t wait;
    extern double rtemsTicksPerSecond_double;
    
    /*
     * Convert time to ticks
     */
    if (timeout <= 0.0) {
        wait = RTEMS_NO_WAIT;
        delay = 0;
    }
    else {
        wait = RTEMS_WAIT;
        delay = (int)(timeout * rtemsTicksPerSecond_double);
        if (delay == 0)
            delay++;
    }
    return receiveMessage(id, message, size, wait, delay);
}

epicsShareFunc int epicsShareAPI epicsMessageQueuePending(
            epicsMessageQueueId id)
{
    uint32_t count;
    rtems_status_code sc;
    
    sc = rtems_message_queue_get_number_pending(id->id, &count);
    if (sc != RTEMS_SUCCESSFUL) {
        errlogPrintf("Message queue %x get number pending failed: %s\n",
                                                        (unsigned int)id,
                                                        rtems_status_text(sc));
        return -1;
    }
    return count;
}

epicsShareFunc void epicsShareAPI epicsMessageQueueShow(
            epicsMessageQueueId id,
                int level)
{
    int pending = epicsMessageQueuePending(id);
    if (pending >= 0)
        printf ("Message queue %lx -- Pending: %d\n", (unsigned long)id, pending);
}
