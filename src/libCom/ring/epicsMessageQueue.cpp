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
 *      $Id$
 *
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

/*
 * Interthread message passing
 *
 * Machines with 'traditional' memory access semantics could get by without
 * the queueMutex for single producer/consumers, but architectures, such as
 * symmetric-multiprocessing machines with out-of-order writes, require
 * mutex locks to ensure correct operation.  I decided to always use the
 * mutex rather having two versions of this code.
 */
#include "epicsMessageQueue.h"
#include <epicsAssert.h>
#include <memory.h>
#include <stdexcept>

epicsMessageQueue::epicsMessageQueue(unsigned int capacity,
                                     unsigned int maxMessageSize) 
{
    assert(capacity != 0);
    assert(maxMessageSize != 0);
    this->capacity = capacity;
    this->maxMessageSize = maxMessageSize;
    slotSize = (sizeof(unsigned long)
              + maxMessageSize + sizeof(unsigned long) - 1)
                                                     / sizeof(unsigned long);
    buf = new unsigned long[capacity * slotSize];
    inPtr = outPtr = firstMessageSlot = (char *)&buf[0];
    lastMessageSlot = (char *)&buf[(capacity - 1) * slotSize];
    slotSize *= sizeof(unsigned long);
    full = false;
}

epicsMessageQueue::~epicsMessageQueue()
{
    delete buf;
}

bool
epicsMessageQueue::send(void *message, unsigned int size)
{
    char *inPtr, *outPtr, *nextPtr;

    assert(size <= maxMessageSize);
    queueMutex.lock();
    if (full) {
        queueMutex.unlock();
        return false;
    }
    inPtr = (char *)this->inPtr;
    outPtr = (char *)this->outPtr;
    if (inPtr == lastMessageSlot)
        nextPtr = firstMessageSlot;
    else
        nextPtr = inPtr + slotSize;
    if (nextPtr == outPtr)
        full = true;
    *(volatile unsigned long *)inPtr = size;
    memcpy((unsigned long *)inPtr + 1, message, size);
    this->inPtr = nextPtr;
    queueMutex.unlock();
    queueEvent.signal();
    return true;
}

bool
epicsMessageQueue::receive(void *message, unsigned int *size, bool withTimeout, double timeout)
{
    char *outPtr;
    unsigned long l;

    queueMutex.lock();
    outPtr = (char *)this->outPtr;
    while ((outPtr == inPtr) && !full) {
        queueMutex.unlock();
        if (withTimeout) {
            if (queueEvent.wait(timeout) == false)
                return false;
        }
        else {
            queueEvent.wait();
        }
        queueMutex.lock();
        outPtr = (char *)this->outPtr;
    }
    *size = l = *(unsigned long *)outPtr;
    memcpy(message, (unsigned long *)outPtr + 1, l);
    if (outPtr == lastMessageSlot)
        outPtr = firstMessageSlot;
    else
        outPtr = outPtr + slotSize;
    this->outPtr = outPtr;
    full = false;
    queueMutex.unlock();
    return true;
}

void
epicsMessageQueue::receive(void *message, unsigned int *size)
{
    this->receive(message, size, false, 0.0);
}

bool
epicsMessageQueue::receive(void *message, unsigned int *size, double timeout)
{
    return this->receive(message, size, true, timeout);
}

bool
epicsMessageQueue::isFull() const
{
    return full;
}

bool
epicsMessageQueue::isEmpty()
{
    bool empty;

    queueMutex.lock();
    empty = ((outPtr == inPtr) && !full);
    queueMutex.unlock();
    return empty;
}

void
epicsMessageQueue::show(unsigned int level) const
{
    char *inPtr, *outPtr;
    int nmsg;

    inPtr = (char *)this->inPtr;
    outPtr = (char *)this->outPtr;
    if (inPtr >= outPtr)
        nmsg = (inPtr - outPtr) / slotSize;
    else
        nmsg = capacity  - (outPtr - inPtr) / slotSize;
    if (full)
        nmsg = capacity;
    printf("Message Queue Used:%d  Slots:%d", nmsg, capacity);
    if (level >= 1)
        printf("  Maximum size:%u", maxMessageSize);
    printf("\n");
}

extern "C" {

epicsShareFunc epicsMessageQueueId epicsShareAPI epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize)
{
    epicsMessageQueue *id;
    
    try {
        id = new epicsMessageQueue::epicsMessageQueue(capacity, maximumMessageSize);
    }
    catch (...) {
        return NULL;
    }
    return id;
}

epicsShareFunc void epicsShareAPI epicsMessageQueueDestroy(
    epicsMessageQueueId id)
{
    delete (epicsMessageQueue *)id;
}

epicsShareFunc int epicsShareAPI epicsMessageQueueSend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize)
{
    return ((epicsMessageQueue *)id)->send(message, messageSize);
}

epicsShareFunc void epicsShareAPI epicsMessageQueueReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int *messageSize)
{
    ((epicsMessageQueue *)id)->receive(message, messageSize);
}

epicsShareFunc int epicsShareAPI epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int *messageSize,
    double timeout)
{
    return ((epicsMessageQueue *)id)->receive(message, messageSize, timeout);
}

epicsShareFunc int epicsShareAPI epicsMessageQueueIsFull(
    epicsMessageQueueId id)
{
    return ((epicsMessageQueue *)id)->isFull();
}

epicsShareFunc int epicsShareAPI epicsMessageQueueIsEmpty(
    epicsMessageQueueId id)
{
    return ((epicsMessageQueue *)id)->isEmpty();
}

epicsShareFunc void epicsShareAPI epicsMessageQueueShow(
    epicsMessageQueueId id,
    unsigned int level)
{
    ((epicsMessageQueue *)id)->show(level);
}

} /* extern "C" */
