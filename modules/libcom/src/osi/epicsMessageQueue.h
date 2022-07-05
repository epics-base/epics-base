/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsMessageQueue.h
 * \author W. Eric Norum
 *
 * \brief A C++ and a C facility for communication between threads.
 *
 * Each C function corresponds to one of the C++ methods.
 */

#ifndef epicsMessageQueueh
#define epicsMessageQueueh

#include "epicsAssert.h"
#include "libComAPI.h"

typedef struct epicsMessageQueueOSD *epicsMessageQueueId;

#ifdef __cplusplus

/** Provides methods for sending messages between threads on a first in,
 *  first out basis. It is designed so that a single message queue can
 *  be used with multiple writer and reader threads.
 *
 *  A C++ epicsMessageQueue cannot be assigned to, copy-constructed, or
 *  constructed without giving the capacity and max-imumMessageSize
 *  arguments.
 *
 *  The C++ compiler will object to some of the statements below:
 *  \code{.cpp}
 *  epicsMessageQueue mq0();   // Error: default constructor is private
 *  epicsMessageQueue mq1(10, 20); // OK
 *  epicsMessageQueue mq2(t1); // Error: copy constructor is private
 *  epicsMessageQueue*pmq;    // OK, pointer
 *  *pmq = mq1;               // Error: assignment operator is private
 *  pmq = &mq1;               // OK, pointer assignment and address-of
 *  \endcode
 **/
class LIBCOM_API epicsMessageQueue {
public:

    /**
     *  \brief Constructor.
     *  \param capacity  Maximum number of messages to queue
     *  \param maximumMessageSize  Number of bytes of the largest
     *  message that may be queued
     **/
    epicsMessageQueue ( unsigned int capacity,
                        unsigned int maximumMessageSize );

    /**
     *  \brief Destructor.
     **/
    ~epicsMessageQueue ();

    /**
     *  \brief Try to send a message.
     *  \note On VxWorks and RTEMS this method may be called from
     *  an interrupt handler.
     *  \returns 0 if the message was sent to a receiver or queued for
     *  future delivery.
     *  \returns -1 if no more messages can be queued or if the message
     *  is larger than the queue's maximum message size.
     **/
    int trySend ( void *message, unsigned int messageSize );

    /**
     *  \brief Send a message.
     *  \returns 0 if the message was sent to a receiver or queued for
     *  future delivery.
     *  \returns -1 if the message is larger than the queue's maximum
     *  message size.
     **/
    int send ( void *message, unsigned int messageSize );

    /**
     *  \brief Send a message or timeout.
     *  \param message Pointer to the message to be sent
     *  \param messageSize The size of \p message
     *  \param timeout The timeout delay in seconds. A timeout of zero is
     *  equivalent to calling trySend(); NaN or any value too large to be
     *  represented to the target OS is equivalent to no timeout.
     *  \returns 0 if the message was sent to a receiver or queued for
     *  future delivery.
     *  \returns -1 if the timeout was reached before the
     *  message could be sent or queued, or if the message is larger
     *  than the queue's maximum message size.
     **/
    int send ( void *message, unsigned int messageSize, double timeout );

    /**
     *  \brief Try to receive a message.
     *  \param[out] message Output buffer to store the received message
     *  \param size Size of the buffer pointed to by \p message
     *
     *  If the queue holds at least one message,
     *  the first message on the queue is moved to the specified location
     *  and the length of that message is returned.
     *
     *  If the received message is larger than the specified message size
     *  the implementation may either return -1, or truncate the
     *  message. It is most efficient if the messageBufferSize is equal
     *  to the maximumMessageSize with which the message queue was
     *  created.
     *  \returns Number of bytes in the message.
     *  \returns -1 if the message queue is empty, or the buffer too small.
     **/
    int tryReceive ( void *message, unsigned int size );

    /**
     *  \brief Fetch the next message on the queue.
     *  \param[out] message Output buffer to store the received message
     *  \param size Size of the buffer pointed to by \p message
     *
     *  Wait for a message to be sent if the queue is empty, then move
     *  the first message queued to the specified location.
     *
     *  If the received message is larger than the specified message size
     *  the implementation may either return -1, or truncate the
     *  message. It is most efficient if the messageBufferSize is equal
     *  to the maximumMessageSize with which the message queue was
     *  created.
     *  \returns Number of bytes in the message.
     *  \returns -1 if the buffer is too small for the message.
     **/
    int receive ( void *message, unsigned int size );

    /**
     *  \brief Wait for and fetch the next message.
     *  \param[out] message Output buffer to store the received message
     *  \param size Size of the buffer pointed to by \p message
     *  \param timeout The timeout delay in seconds. A timeout of zero is
     *  equivalent to calling tryReceive(); NaN or any value too large to
     *  be represented to the target OS is equivalent to no timeout.
     *
     *  Waits up to \p timeout seconds for a message to arrive if the queue
     *  is empty, then moves the first message to the message buffer.
     *
     *  If the received message is larger than the buffer size
     *  the implementation may either return -1, or truncate the
     *  message. It is most efficient if the messageBufferSize is equal
     *  to the maximumMessageSize with which the message queue was
     *  created.
     *  \returns Number of bytes in the message.
     *  \returns -1 if a message is not received within the timeout
     *  interval.
     **/
    int receive ( void *message, unsigned int size, double timeout );

    /**
     *  \brief Displays some information about the message queue.
     *  \param level Controls the amount of information displayed.
     **/
    void show ( unsigned int level = 0 );

    /**
     *  \brief How many messages are queued.
     *  \returns The number of messages presently in the queue.
     **/
    unsigned int pending ();

private:
    /**
     *  Prevent compiler-generated member functions default constructor,
     *  copy constructor, assignment operator.
     **/
    epicsMessageQueue();
    epicsMessageQueue(const epicsMessageQueue &);
    epicsMessageQueue& operator=(const epicsMessageQueue &);

    epicsMessageQueueId id;
};

extern "C" {
#endif /*__cplusplus */

/**
 *  \brief Create a message queue.
 *  \param capacity  Maximum number of messages to queue
 *  \param maximumMessageSize  Number of bytes of the largest
 *  message that may be queued
 *  \return An identifier for the new queue, or 0.
 **/
LIBCOM_API epicsMessageQueueId epicsStdCall epicsMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize);

/**
 *  \brief Destroy a message queue, release all its memory.
 **/
LIBCOM_API void epicsStdCall epicsMessageQueueDestroy(
    epicsMessageQueueId id);

/**
 *  \brief Try to send a message.
 *  \note On VxWorks and RTEMS this routine may be called from
 *  an interrupt handler.
 *  \returns 0 if the message was sent to a receiver or queued for
 *  future delivery.
 *  \returns -1 if no more messages can be queued or if the message
 *  is larger than the queue's maximum message size.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueTrySend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize);

/**
 *  \brief Send a message.
 *  \returns 0 if the message was sent to a receiver or queued for
 *  future delivery.
 *  \returns -1 if the message is larger than the queue's maximum
 *  message size.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueSend(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize);

/**
 *  \brief Send a message or timeout.
 *  \returns 0 if the message was sent to a receiver or queued for
 *  future delivery.
 *  \returns -1 if the timeout was reached before the
 *  message could be sent or queued, or if the message is larger
 *  than the queue's maximum message size.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueSendWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int messageSize,
    double timeout);

/**
 *  \brief Try to receive a message.
 *
 *  If the queue holds at least one message,
 *  the first message on the queue is moved to the specified location
 *  and the length of that message is returned.
 *
 *  If the received message is larger than the specified message size
 *  the implementation may either return -1, or truncate the
 *  message. It is most efficient if the messageBufferSize is equal
 *  to the maximumMessageSize with which the message queue was
 *  created.
 *  \returns Number of bytes in the message.
 *  \returns -1 if the message queue is empty, or the buffer too small.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueTryReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size);

/**
 *  \brief Fetch the next message on the queue.
 *
 *  Wait for a message to be sent if the queue is empty, then move the
 *  first message queued to the specified location.
 *
 *  If the received message is larger than the specified message size
 *  the implementation may either return -1, or truncate the
 *  message. It is most efficient if the messageBufferSize is equal
 *  to the maximumMessageSize with which the message queue was
 *  created.
 *  \returns Number of bytes in the message.
 *  \returns -1 if the buffer is too small for the message.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueReceive(
    epicsMessageQueueId id,
    void *message,
    unsigned int size);

/**
 *  \brief Wait for a message to be queued.
 *
 *  Wait up to \p timeout seconds for a message to be sent if the queue
 *  is empty, then move the first message to the specified location.
 *
 *  If the received message is larger than the specified message buffer
 *  size the implementation may either return -1, or truncate the
 *  message. It is most efficient if the messageBufferSize is equal
 *  to the maximumMessageSize with which the message queue was
 *  created.
 *  \returns Number of bytes in the message.
 *  \returns -1 if a message is not received within the timeout
 *  interval.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueueReceiveWithTimeout(
    epicsMessageQueueId id,
    void *message,
    unsigned int size,
    double timeout);

/**
 *  \brief How many messages are queued.
 *  \param id Message queue identifier.
 *  \returns The number of messages presently in the queue.
 **/
LIBCOM_API int epicsStdCall epicsMessageQueuePending(
    epicsMessageQueueId id);

/**
 *  \brief Displays some information about the message queue.
 *  \param id Message queue identifier.
 *  \param level Controls the amount of information displayed.
 **/
LIBCOM_API void epicsStdCall epicsMessageQueueShow(
    epicsMessageQueueId id,
    int level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "osdMessageQueue.h"

#endif /* epicsMessageQueueh */
