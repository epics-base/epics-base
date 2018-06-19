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

#include <stdexcept>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsMessageQueue.h"
#include <ellLib.h>
#include <epicsAssert.h>
#include <epicsEvent.h>
#include <epicsMutex.h>

/*
 * Event cache
 */
struct eventNode {
    ELLNODE         link;
    epicsEventId    event;
};

/*
 * List of threads waiting to send or receive a message
 */
struct threadNode {
    ELLNODE             link;
    struct eventNode   *evp;
    void               *buf;
    unsigned int        size;
    volatile bool       eventSent;
};

/*
 * Message info
 */
struct epicsMessageQueueOSD {
    ELLLIST         sendQueue;
    ELLLIST         receiveQueue;
    ELLLIST         eventFreeList;
    int             numberOfSendersWaiting;
    
    epicsMutexId    mutex;
    unsigned long   capacity;
    unsigned long   maxMessageSize;

    unsigned long  *buf;
    char           *firstMessageSlot;
    char           *lastMessageSlot;
    volatile char  *inPtr;
    volatile char  *outPtr;
    unsigned long   slotSize;

    bool            full;
};

epicsShareFunc epicsMessageQueueId epicsShareAPI epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maxMessageSize)
{
    epicsMessageQueueId pmsg;
    unsigned int slotBytes, slotLongs;

    if(capacity == 0)
        return NULL;

    pmsg = (epicsMessageQueueId)calloc(1, sizeof(*pmsg));
    if(!pmsg)
        return NULL;

    pmsg->capacity = capacity;
    pmsg->maxMessageSize = maxMessageSize;
    slotLongs = 1 + ((maxMessageSize + sizeof(unsigned long) - 1) / sizeof(unsigned long));
    slotBytes = slotLongs * sizeof(unsigned long);

    pmsg->mutex = epicsMutexCreate();
    pmsg->buf = (unsigned long*)calloc(pmsg->capacity, slotBytes);
    if(!pmsg->buf || !pmsg->mutex) {
        if(pmsg->mutex)
            epicsMutexDestroy(pmsg->mutex);
        free(pmsg->buf);
        free(pmsg);
        return NULL;
    }

    pmsg->inPtr = pmsg->outPtr = pmsg->firstMessageSlot = (char *)&pmsg->buf[0];
    pmsg->lastMessageSlot = (char *)&pmsg->buf[(capacity - 1) * slotLongs];
    pmsg->full = false;
    pmsg->slotSize = slotBytes;

    ellInit(&pmsg->sendQueue);
    ellInit(&pmsg->receiveQueue);
    ellInit(&pmsg->eventFreeList);
    return pmsg;
}

static void
freeEventNode(struct eventNode *enode)
{
    epicsEventDestroy(enode->event);
    free(enode);
}

epicsShareFunc void epicsShareAPI
epicsMessageQueueDestroy(epicsMessageQueueId pmsg)
{
    struct eventNode *evp;

    while ((evp = reinterpret_cast < struct eventNode * >
            ( ellGet(&pmsg->eventFreeList) ) ) != NULL) {
        freeEventNode(evp);
    }
    epicsMutexDestroy(pmsg->mutex);
    free(pmsg->buf);
    free(pmsg);
}

static struct eventNode *
getEventNode(epicsMessageQueueId pmsg)
{
    struct eventNode *evp;

    evp = reinterpret_cast < struct eventNode * > ( ellGet(&pmsg->eventFreeList) );
    if (evp == NULL) {
        evp = (struct eventNode *) calloc(1, sizeof(*evp));
        if (evp) {
            evp->event = epicsEventCreate(epicsEventEmpty);
            if (evp->event == NULL) {
                free(evp);
                return NULL;
            }
        }
    }
    return evp;
}

static int
mySend(epicsMessageQueueId pmsg, void *message, unsigned int size,
    double timeout)
{
    char *myInPtr, *nextPtr;
    struct threadNode *pthr;

    if(size > pmsg->maxMessageSize)
        return -1;

    /*
     * See if message can be sent
     */
    epicsMutexMustLock(pmsg->mutex);

    if ((pmsg->numberOfSendersWaiting > 0)
     || (pmsg->full && (ellFirst(&pmsg->receiveQueue) == NULL))) {
        /*
         * Return if not allowed to wait
         */
        if (timeout == 0) {
            epicsMutexUnlock(pmsg->mutex);
            return -1;
        }

        /*
         * Wait
         */
        struct threadNode threadNode;
        threadNode.evp = getEventNode(pmsg);
        threadNode.eventSent = false;
        if (!threadNode.evp) {
            epicsMutexUnlock(pmsg->mutex);
            return -1;
        }

        ellAdd(&pmsg->sendQueue, &threadNode.link);
        pmsg->numberOfSendersWaiting++;

        epicsMutexUnlock(pmsg->mutex);

        epicsEventStatus status;
        if (timeout > 0)
            status = epicsEventWaitWithTimeout(threadNode.evp->event, timeout);
        else
            status = epicsEventWait(threadNode.evp->event);

        epicsMutexMustLock(pmsg->mutex);

        if(!threadNode.eventSent)
            ellDelete(&pmsg->sendQueue, &threadNode.link);
        pmsg->numberOfSendersWaiting--;

        ellAdd(&pmsg->eventFreeList, &threadNode.evp->link);

        if ((pmsg->full && (ellFirst(&pmsg->receiveQueue) == NULL)) ||
            status != epicsEventOK) {
            epicsMutexUnlock(pmsg->mutex);
            return -1;
        }
    }

    /*
     * Copy message to waiting receiver
     */
    if ((pthr = reinterpret_cast < struct threadNode * >
         ( ellGet(&pmsg->receiveQueue) ) ) != NULL) {
        if(size <= pthr->size)
            memcpy(pthr->buf, message, size);
        pthr->size = size;
        pthr->eventSent = true;
        epicsEventSignal(pthr->evp->event);
        epicsMutexUnlock(pmsg->mutex);
        return 0;
    }

    /*
     * Copy to queue
     */
    myInPtr = (char *)pmsg->inPtr;
    if (myInPtr == pmsg->lastMessageSlot)
        nextPtr = pmsg->firstMessageSlot;
    else
        nextPtr = myInPtr + pmsg->slotSize;
    if (nextPtr == (char *)pmsg->outPtr)
        pmsg->full = true;
    *(volatile unsigned long *)myInPtr = size;
    memcpy((unsigned long *)myInPtr + 1, message, size);
    pmsg->inPtr = nextPtr;
    epicsMutexUnlock(pmsg->mutex);
    return 0;
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueTrySend(epicsMessageQueueId pmsg, void *message,
    unsigned int size)
{
    return mySend(pmsg, message, size, 0);
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueSend(epicsMessageQueueId pmsg, void *message,
    unsigned int size)
{
    return mySend(pmsg, message, size, -1);
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueSendWithTimeout(epicsMessageQueueId pmsg, void *message,
    unsigned int size, double timeout)
{
    return mySend(pmsg, message, size, timeout);
}

static int
myReceive(epicsMessageQueueId pmsg, void *message, unsigned int size,
    double timeout)
{
    char *myOutPtr;
    unsigned long l;
    struct threadNode *pthr;

    /*
     * If there's a message on the queue, copy it
     */
    epicsMutexMustLock(pmsg->mutex);

    myOutPtr = (char *)pmsg->outPtr;
    if ((myOutPtr != pmsg->inPtr) || pmsg->full) {
        int ret;
        l = *(unsigned long *)myOutPtr;
        if (l <= size) {
            memcpy(message, (unsigned long *)myOutPtr + 1, l);
            ret = l;
        }
        else {
            ret = -1;
        }
        if (myOutPtr == pmsg->lastMessageSlot)
            pmsg->outPtr = pmsg->firstMessageSlot;
        else
            pmsg->outPtr += pmsg->slotSize;
        pmsg->full = false;

        /*
         * Wake up the oldest task waiting to send
         */
        if ((pthr = reinterpret_cast < struct threadNode * >
             ( ellGet(&pmsg->sendQueue) ) ) != NULL) {
            pthr->eventSent = true;
            epicsEventSignal(pthr->evp->event);
        }
        epicsMutexUnlock(pmsg->mutex);
        return ret;
    }

    /*
     * Return if not allowed to wait
     */
    if (timeout == 0) {
        epicsMutexUnlock(pmsg->mutex);
        return -1;
    }

    /*
     * Wake up the oldest task waiting to send
     */
    if ((pthr = reinterpret_cast < struct threadNode * >
         ( ellGet(&pmsg->sendQueue) ) ) != NULL) {
        pthr->eventSent = true;
        epicsEventSignal(pthr->evp->event);
    }

    /*
     * Wait for message to arrive
     */
    struct threadNode threadNode;
    threadNode.evp = getEventNode(pmsg);
    threadNode.buf = message;
    threadNode.size = size;
    threadNode.eventSent = false;

    if (!threadNode.evp) {
        epicsMutexUnlock(pmsg->mutex);
        return -1;
    }

    ellAdd(&pmsg->receiveQueue, &threadNode.link);
    epicsMutexUnlock(pmsg->mutex);

    epicsEventStatus status;
    if (timeout > 0)
        status = epicsEventWaitWithTimeout(threadNode.evp->event, timeout);
    else
        status = epicsEventWait(threadNode.evp->event);

    epicsMutexMustLock(pmsg->mutex);

    if (!threadNode.eventSent)
        ellDelete(&pmsg->receiveQueue, &threadNode.link);
    ellAdd(&pmsg->eventFreeList, &threadNode.evp->link);

    epicsMutexUnlock(pmsg->mutex);

    if (threadNode.eventSent && (threadNode.size <= size) &&
        status == epicsEventOK)
        return threadNode.size;
    return -1;
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueTryReceive(epicsMessageQueueId pmsg, void *message,
    unsigned int size)
{
    return myReceive(pmsg, message, size, 0);
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueReceive(epicsMessageQueueId pmsg, void *message,
    unsigned int size)
{
    return myReceive(pmsg, message, size, -1);
}

epicsShareFunc int epicsShareAPI
epicsMessageQueueReceiveWithTimeout(epicsMessageQueueId pmsg, void *message,
    unsigned int size, double timeout)
{
    return myReceive(pmsg, message, size, timeout);
}

epicsShareFunc int epicsShareAPI
epicsMessageQueuePending(epicsMessageQueueId pmsg)
{
    char *myInPtr, *myOutPtr;
    int nmsg;

    epicsMutexMustLock(pmsg->mutex);
    myInPtr = (char *)pmsg->inPtr;
    myOutPtr = (char *)pmsg->outPtr;
    if (pmsg->full)
        nmsg = pmsg->capacity;
    else if (myInPtr >= myOutPtr)
        nmsg = (myInPtr - myOutPtr) / pmsg->slotSize;
    else
        nmsg = pmsg->capacity  - (myOutPtr - myInPtr) / pmsg->slotSize;
    epicsMutexUnlock(pmsg->mutex);
    return nmsg;
}

epicsShareFunc void epicsShareAPI
epicsMessageQueueShow(epicsMessageQueueId pmsg, int level)
{
    printf("Message Queue Used:%d  Slots:%lu", epicsMessageQueuePending(pmsg), pmsg->capacity);
    if (level >= 1)
        printf("  Maximum size:%lu", pmsg->maxMessageSize);
    printf("\n");
}
