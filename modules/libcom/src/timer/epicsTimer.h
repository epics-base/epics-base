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
 * \file epicsTimer.h
 * \brief An API for managing timers.
 * \author  Marty Kraimer, Jeff Hill
 *
 * API for managing timers with different characteristics and scheduling 
 * mechanisms, through the use of callback functions.
 * The epicsTimer does not hold its lock when calling callbacks.
 */
#ifndef epicsTimerH
#define epicsTimerH

#include <float.h>

#include "libComAPI.h"
#include "epicsTime.h"
#include "epicsThread.h"

#ifdef __cplusplus

/** \brief Defining the callback function to be called when a timer expires, and its return value.
 * Any code that uses the timer must implement this class.
 * returning "noRestart" or "expireStatus ( restart, 30.0 ) */
class LIBCOM_API epicsTimerNotify {
public:
    enum restart_t { noRestart, restart };
    class expireStatus {
    public:
        LIBCOM_API expireStatus ( restart_t );
        LIBCOM_API expireStatus ( restart_t, const double & expireDelaySec );
        LIBCOM_API bool restart () const;
        LIBCOM_API double expirationDelay () const;
    private:
        double delay;
    };
    virtual ~epicsTimerNotify () = 0;
    virtual expireStatus expire ( const epicsTime & currentTime ) = 0;
    virtual void show ( unsigned int level ) const;
};

/** \brief Defining the methods for managing a timer.
 * Its member functions include start, cancel, and destroy.
 * WARNING: A deadlock will occur if you hold a lock while
 * calling this function that you also take within the timer
 * expiration callback. */
class LIBCOM_API epicsTimer {
public:
    virtual void destroy () = 0;
    virtual void start ( epicsTimerNotify &, const epicsTime & ) = 0;
    virtual void start ( epicsTimerNotify &, double delaySeconds ) = 0;
    virtual void cancel () = 0;
    struct expireInfo {
        expireInfo ( bool active, const epicsTime & expireTime );
        bool active;
        epicsTime expireTime;
    };
    virtual expireInfo getExpireInfo () const = 0;
    double getExpireDelay ();
    virtual void show ( unsigned int level ) const = 0;
protected:
    virtual ~epicsTimer () = 0;
};

/** \brief Defining the interface for a timer queue,
 * which holds the set of timers and schedules them for execution. */
class epicsTimerQueue {
public:
    virtual epicsTimer & createTimer () = 0;
    virtual void show ( unsigned int level ) const = 0;
protected:
    LIBCOM_API virtual ~epicsTimerQueue () = 0;
};

/** \brief Derived from epicsTimerQueue, providing the implementation
 * of an active timer queue, which runs as a thread and schedules timers
 * based on their expiration times. */
class epicsTimerQueueActive
    : public epicsTimerQueue {
public:
    static LIBCOM_API epicsTimerQueueActive & allocate (
        bool okToShare, unsigned threadPriority = epicsThreadPriorityMin + 10 );
    virtual void release () = 0;
protected:
    LIBCOM_API virtual ~epicsTimerQueueActive () = 0;
};

/** \brief Defining the interface for a callback function to be called when
 * a timer is inserted into the queue and the delay to the next expire has changed
 * or when a timer is removed from the queue.
 * If there is a quantum in the scheduling of timer intervals
 * return this quantum in seconds. If unknown then return zero.*/
class epicsTimerQueueNotify {
public:
    virtual void reschedule () = 0;
    virtual double quantum () = 0;
protected:
    LIBCOM_API virtual ~epicsTimerQueueNotify () = 0;
};

/** \brief Derived from epicsTimerQueue, providing a passive timer queue,
 * which is driven by the caller and processes timers one at a time. */
class epicsTimerQueuePassive
    : public epicsTimerQueue {
public:
    static LIBCOM_API epicsTimerQueuePassive & create ( epicsTimerQueueNotify & );
    LIBCOM_API virtual ~epicsTimerQueuePassive () = 0;
    virtual double process ( const epicsTime & currentTime ) = 0;
};

inline epicsTimer::expireInfo::expireInfo ( bool activeIn,
    const epicsTime & expireTimeIn ) :
        active ( activeIn ), expireTime ( expireTimeIn )
{
}

inline double epicsTimer::getExpireDelay ()
{
    epicsTimer::expireInfo info = this->getExpireInfo ();
    if ( info.active ) {
        double delay = info.expireTime - epicsTime::getCurrent ();
        if ( delay < 0.0 ) {
            delay = 0.0;
        }
        return delay;
    }
    return - DBL_MAX;
}

extern "C" {
#endif

/** \brief pointer to identifz the timer */
typedef struct epicsTimerForC * epicsTimerId;
/** \brief pointer to callback function, called after expire */
typedef void ( *epicsTimerCallback ) ( void *pPrivate );

/** \brief A thread-managed queue for active timers */
typedef struct epicsTimerQueueActiveForC * epicsTimerQueueId;
LIBCOM_API epicsTimerQueueId epicsStdCall
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority );
LIBCOM_API void epicsStdCall 
    epicsTimerQueueRelease ( epicsTimerQueueId );
LIBCOM_API epicsTimerId epicsStdCall 
    epicsTimerQueueCreateTimer ( epicsTimerQueueId queueid,
        epicsTimerCallback callback, void *arg );
LIBCOM_API void epicsStdCall 
    epicsTimerQueueDestroyTimer ( epicsTimerQueueId queueid, epicsTimerId id );
LIBCOM_API void  epicsStdCall 
    epicsTimerQueueShow ( epicsTimerQueueId id, unsigned int level );

/** \brief A passive queue for timers that are driven by the caller */
typedef struct epicsTimerQueuePassiveForC * epicsTimerQueuePassiveId;
typedef void ( * epicsTimerQueueNotifyReschedule ) ( void * pPrivate );
typedef double ( * epicsTimerQueueNotifyQuantum ) ( void * pPrivate );
LIBCOM_API epicsTimerQueuePassiveId epicsStdCall
    epicsTimerQueuePassiveCreate ( epicsTimerQueueNotifyReschedule,
        epicsTimerQueueNotifyQuantum, void *pPrivate );
LIBCOM_API void epicsStdCall 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId );
LIBCOM_API epicsTimerId epicsStdCall 
    epicsTimerQueuePassiveCreateTimer (
        epicsTimerQueuePassiveId queueid, epicsTimerCallback pCallback, void *pArg );
LIBCOM_API void epicsStdCall 
    epicsTimerQueuePassiveDestroyTimer ( epicsTimerQueuePassiveId queueid, epicsTimerId id );
LIBCOM_API double epicsStdCall 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId );
LIBCOM_API void  epicsStdCall 
    epicsTimerQueuePassiveShow ( epicsTimerQueuePassiveId id, unsigned int level );

LIBCOM_API void epicsStdCall 
    epicsTimerStartTime ( epicsTimerId id, const epicsTimeStamp *pTime );
LIBCOM_API void epicsStdCall 
    epicsTimerStartDelay ( epicsTimerId id, double delaySeconds );
LIBCOM_API void epicsStdCall 
    epicsTimerCancel ( epicsTimerId id );
LIBCOM_API double epicsStdCall 
    epicsTimerGetExpireDelay ( epicsTimerId id );
LIBCOM_API void  epicsStdCall 
    epicsTimerShow ( epicsTimerId id, unsigned int level );

#ifdef __cplusplus
}
#endif

#endif /* epicsTimerH */
