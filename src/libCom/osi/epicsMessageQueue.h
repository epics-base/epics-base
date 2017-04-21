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
 * Interthread message passing
 */
#ifndef epicsMessageQueueh
#define epicsMessageQueueh

#include "epicsAssert.h"
#include "shareLib.h"

typedef struct epicsMessageQueueOSD *epicsMessageQueueId;

#ifdef __cplusplus

class epicsShareClass epicsMessageQueue {
public:
    epicsMessageQueue ( unsigned int capacity,
                        unsigned int maximumMessageSize );
    ~epicsMessageQueue ();
    int trySend ( void *message, unsigned int messageSize );
    int send ( void *message, unsigned int messageSize);
    int send ( void *message, unsigned int messageSize, double timeout );
    int tryReceive ( void *message, unsigned int size );
    int receive ( void *message, unsigned int size );
    int receive ( void *message, unsigned int size, double timeout );
    void show ( unsigned int level = 0 );
    unsigned int pending ();

private: /* Prevent compiler-generated member functions */
    /* default constructor, copy constructor, assignment operator */
    epicsMessageQueue();
    epicsMessageQueue(const epicsMessageQueue &);
    epicsMessageQueue& operator=(const epicsMessageQueue &);

    epicsMessageQueueId id;
};

extern "C" {
#endif /*__cplusplus */

epicsShareFunc epicsMessageQueueId epicsShareAPI epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize);
epicsShareFunc void epicsShareAPI epicsMessageQueueDestroy(
    epicsMessageQueueId id);
epicsShareFunc int epicsShareAPI epicsMessageQueueTrySend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize);
epicsShareFunc int epicsShareAPI epicsMessageQueueSend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize);
epicsShareFunc int epicsShareAPI epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize,
    double timeout);
epicsShareFunc int epicsShareAPI epicsMessageQueueTryReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size);
epicsShareFunc int epicsShareAPI epicsMessageQueueReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size);
epicsShareFunc int epicsShareAPI epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout);
epicsShareFunc int epicsShareAPI epicsMessageQueuePending(
    epicsMessageQueueId id);
epicsShareFunc void epicsShareAPI epicsMessageQueueShow(
    epicsMessageQueueId id,
    int level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "osdMessageQueue.h"

#endif /* epicsMessageQueueh */
