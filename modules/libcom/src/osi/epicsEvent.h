/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**\file epicsEvent.h
 *
 * \brief APIs for the epicsEvent binary semaphore.
 *
 * Defines the C++ and C API's for a simple binary semaphore. If multiple threads are
 * waiting on the same event, only one of them will be woken when the event is signaled.
 *
 * The primary use of an event semaphore is for thread synchronization. An example of using an
 * event semaphore is a consumer thread that processes requests from one or more producer threads.
 * For example:
 *
 * When creating the consumer thread also create an epicsEvent.
 \code
   epicsEvent event;
 \endcode
 * The consumer thread has code containing:
 \code
       while(1) {
           pevent.wait();
           while( {more work} ) {
               {process work}
           }
       }
 \endcode
 * Producers create requests and issue the statement:
 \code
       pevent.trigger();
 \endcode
 **/

#ifndef epicsEventh
#define epicsEventh

#include "libComAPI.h"

/** \brief An identifier for an epicsEvent for use with the C API */
typedef struct epicsEventOSD *epicsEventId;

/** \brief Return status from several C API routines. */
typedef enum {
    epicsEventOK = 0,
    epicsEventWaitTimeout,
    epicsEventError
} epicsEventStatus;

/** \brief Old name provided for backwards compatibility */
#define epicsEventWaitStatus epicsEventStatus
/** \brief Old name provided for backwards compatibility */
#define epicsEventWaitOK epicsEventOK
/** \brief Old name provided for backwards compatibility */
#define epicsEventWaitError epicsEventError

/** \brief Possible initial states of a new epicsEvent */
typedef enum {
    epicsEventEmpty,
    epicsEventFull
} epicsEventInitialState;

#ifdef __cplusplus

/**\brief A binary semaphore.
 *
 * An epicsEvent is a binary semaphore that can be empty or full.
 * When empty, a wait() issued before the next call to trigger() will block.
 * When full, the next call to wait() will empty the event and return
 * immediately. Multiple calls to trigger() may occur between wait() calls
 * but will have the same effect as a single trigger(), filling the event.
 **/
class LIBCOM_API epicsEvent {
public:
    /**\brief Constructor.
     * \param initial State when created, empty (the default) or full.
     **/
    epicsEvent ( epicsEventInitialState initial = epicsEventEmpty );
    /**\brief Destroy the epicsEvent and any resources it holds. No calls to
     * wait() can be active when this call is made.
     **/
    ~epicsEvent ();
    /**\brief Trigger the event i.e. ensures the next or current call to wait
     * completes. This method may be called from a vxWorks or RTEMS interrupt
     * handler.
     **/
    void trigger ();
    /**\brief Signal is a synonym for trigger().
     **/
    void signal () { this->trigger(); }
    /**\brief Wait for the event.
     * \note Blocks until full.
     **/
    void wait ();
    /**\brief Wait for the event or until the specified timeout.
     * \param timeout The timeout delay in seconds. A timeout of zero is
     * equivalent to calling tryWait(); NaN or any value too large to be
     * represented to the target OS is equivalent to no timeout.
     * \return True if the event was triggered, False if it timed out.
     **/
    bool wait ( double timeout );
    /**\brief Similar to wait() except that if the event is currently empty the
     * call will return immediately.
     * \return True if the event was full (triggered), False if empty.
     **/
    bool tryWait ();
    /**\brief Display information about the semaphore.
     * \note The information displayed is architecture dependent.
     * \param level An unsigned int for the level of information to be displayed.
     **/
    void show ( unsigned level ) const;

    class invalidSemaphore;         /* exception payload */
private:
    epicsEvent ( const epicsEvent & );
    epicsEvent & operator = ( const epicsEvent & );
    epicsEventId id;
};

extern "C" {
#endif /*__cplusplus */

/**\brief Create an epicsEvent for use from C code, or return NULL.
 *
 * \param initialState Starting state, \c epicsEventEmpty or \c epicsEventFull.
 * \return An identifier for the new event, or NULL if one not be created.
 **/
LIBCOM_API epicsEventId epicsEventCreate(
    epicsEventInitialState initialState);

/**\brief Create an epicsEvent for use from C code.
 *
 * This routine does not return if the object could not be created.
 * \param initialState Starting state, \c epicsEventEmpty or \c epicsEventFull.
 * \return An identifier for the new event.
 **/
LIBCOM_API epicsEventId epicsEventMustCreate (
    epicsEventInitialState initialState);

/**\brief Destroy an epicsEvent and any resources it holds.
 *
 * No calls to any epicsEventWait routines can be active when this call is made.
 * \param id The event identifier.
 **/
LIBCOM_API void epicsEventDestroy(epicsEventId id);

/**\brief Trigger an event i.e. ensures the next or current call to wait
 * completes.
 *
 * \note This method may be called from a VxWorks or RTEMS interrupt
 * handler.
 * \param id The event identifier.
 * \return Status indicator.
 **/
LIBCOM_API epicsEventStatus epicsEventTrigger(
    epicsEventId id);

/**\brief Trigger an event.
 *
 * This routine does not return if the identifier is invalid.
 * \param id The event identifier.
 */
LIBCOM_API void epicsEventMustTrigger(epicsEventId id);

/**\brief A synonym for epicsEventTrigger().
 * \param ID The event identifier.
 * \return Status indicator.
 **/
#define epicsEventSignal(ID) epicsEventMustTrigger(ID)

/**\brief Wait for an event.
 * \note Blocks until full.
 * \param id The event identifier.
 * \return Status indicator.
 **/
LIBCOM_API epicsEventStatus epicsEventWait(
    epicsEventId id);

/**\brief Wait for an event (see epicsEventWait()).
 *
 * This routine does not return if the identifier is invalid.
 * \param id The event identifier.
 */
LIBCOM_API void epicsEventMustWait(epicsEventId id);

/**\brief Wait an the event or until the specified timeout period is over.
 * \note Blocks until full or timeout.
 * \param id The event identifier.
 * \param timeout The timeout delay in seconds. A timeout of zero is
 * equivalent to calling epicsEventTryWait(); NaN or any value too large
 * to be represented to the target OS is equivalent to no timeout.
 * \return Status indicator.
 **/
LIBCOM_API epicsEventStatus epicsEventWaitWithTimeout(
    epicsEventId id, double timeout);

/**\brief Similar to wait() except that if the event is currently empty the
 * call will return immediately with status \c epicsEventWaitTimeout.
 * \param id The event identifier.
 * \return Status indicator, \c epicsEventWaitTimeout when the event is empty.
 **/
LIBCOM_API epicsEventStatus epicsEventTryWait(
    epicsEventId id);

/**\brief Display information about the semaphore.
 * \note The information displayed is architecture dependent.
 * \param id The event identifier.
 * \param level An unsigned int for the level of information to be displayed.
 **/
LIBCOM_API void epicsEventShow(
    epicsEventId id, unsigned int level);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#include "osdEvent.h"

#endif /* epicsEventh */
