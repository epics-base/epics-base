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
#include <stdexcept>
# include <string.h>

epicsMessageQueue::epicsMessageQueue(unsigned int aCapacity,
                                     unsigned int aMaxMessageSize) 
{
    assert(aCapacity != 0);
    assert(aMaxMessageSize != 0);
    capacity = aCapacity;
    maxMessageSize = aMaxMessageSize;
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
    char *myInPtr, *myOutPtr, *nextPtr;

    assert(size <= maxMessageSize);
    queueMutex.lock();
    if (full) {
        queueMutex.unlock();
        return false;
    }
    myInPtr = (char *)inPtr;
    myOutPtr = (char *)outPtr;
    if (myInPtr == lastMessageSlot)
        nextPtr = firstMessageSlot;
    else
        nextPtr = myInPtr + slotSize;
    if (nextPtr == myOutPtr)
        full = true;
    *(volatile unsigned long *)myInPtr = size;
    memcpy((unsigned long *)myInPtr + 1, message, size);
    this->inPtr = nextPtr;
    queueMutex.unlock();
    queueEvent.signal();
    return true;
}

bool
epicsMessageQueue::receive(void *message, unsigned int *size, bool withTimeout, double timeout)
{
    char *myOutPtr;
    unsigned long l;

    queueMutex.lock();
    myOutPtr = (char *)outPtr;
    while ((myOutPtr == inPtr) && !full) {
        queueMutex.unlock();
        if (withTimeout) {
            if (queueEvent.wait(timeout) == false)
                return false;
        }
        else {
            queueEvent.wait();
        }
        queueMutex.lock();
        myOutPtr = (char *)outPtr;
    }
    *size = l = *(unsigned long *)myOutPtr;
    memcpy(message, (unsigned long *)myOutPtr + 1, l);
    if (myOutPtr == lastMessageSlot)
        myOutPtr = firstMessageSlot;
    else
        myOutPtr = myOutPtr + slotSize;
    this->outPtr = myOutPtr;
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
    char *myInPtr, *myOutPtr;
    int nmsg;

    myInPtr = (char *)inPtr;
    myOutPtr = (char *)outPtr;
    if (myInPtr >= myOutPtr)
        nmsg = (myInPtr - myOutPtr) / slotSize;
    else
        nmsg = capacity  - (myOutPtr - myInPtr) / slotSize;
    if (full)
        nmsg = capacity;
    printf("Message Queue Used:%d  Slots:%d", nmsg, capacity);
    if (level >= 1)
        printf("  Maximum size:%u", maxMessageSize);
    printf("\n");
}

epicsShareFunc epicsMessageQueueId epicsShareAPI epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maxMessageSize)
{
    epicsMessageQueue *qid;
    
    try {
        qid = new epicsMessageQueue(capacity,  maxMessageSize);
    }
    catch (...) {
        return NULL;
    }
    return qid;
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
