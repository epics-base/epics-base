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
 */
#ifndef epicsMessageQueueh
#define epicsMessageQueueh

#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "shareLib.h"

#ifdef __cplusplus

class epicsShareClass epicsMessageQueue {
public:
    epicsMessageQueue ( unsigned int capacity,
                        unsigned int maximumMessageSize );
    ~epicsMessageQueue ();
    bool send ( void *message, unsigned int messageSize );
    bool send ( void *message, unsigned int messageSize, double timeout );
    int receive ( void *message );
    int receive ( void *message, double timeout );
    void show ( unsigned int level = 0 ) const;
    unsigned int pending () const;

private: // Prevent compiler-generated member functions
    // default constructor, copy constructor, assignment operator
    epicsMessageQueue();
    epicsMessageQueue(const epicsMessageQueue &);
    epicsMessageQueue& operator=(const epicsMessageQueue &);

private:
    int receive ( void *message, bool withTimeout, double timeout );

    volatile char  *inPtr;
    volatile char  *outPtr;
    volatile bool   full;
    unsigned int    capacity;
    unsigned int    maxMessageSize;
    unsigned int    slotSize;
    unsigned long  *buf;
    char           *firstMessageSlot;
    char           *lastMessageSlot;
    epicsEvent      queueEvent;
    epicsMutex      queueMutex;
};

extern "C" {
#endif /*__cplusplus */

typedef void *epicsMessageQueueId;

epicsShareFunc epicsMessageQueueId epicsShareAPI epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize);
epicsShareFunc void epicsShareAPI epicsMessageQueueDestroy(
    epicsMessageQueueId id);
epicsShareFunc int epicsShareAPI epicsMessageQueueSend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize);
epicsShareFunc int epicsShareAPI epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize,
    double timeout);
epicsShareFunc int epicsShareAPI epicsMessageQueueReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int *messageSize);
epicsShareFunc int epicsShareAPI epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int *messageSize,
    double timeout);
epicsShareFunc int epicsShareAPI epicsMessageQueuePending(
    epicsMessageQueueId id);
epicsShareFunc void epicsShareAPI epicsMessageQueueShow(
    epicsMessageQueueId id,
    unsigned int level);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /* epicsMessageQueueh */
