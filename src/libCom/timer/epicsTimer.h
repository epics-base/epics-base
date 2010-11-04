/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsTimer.h */

/* Authors: Marty Kraimer, Jeff Hill */

#ifndef epicsTimerH
#define epicsTimerH

#include <float.h>

#include "shareLib.h"
#include "epicsTime.h"
#include "epicsThread.h"

#ifdef __cplusplus

/*
 * Notes:
 * 1) epicsTimer does not hold its lock when calling callbacks.
 */

/* code using a timer must implement epicsTimerNotify */
class epicsShareClass epicsTimerNotify {
public:
    enum restart_t { noRestart, restart };
    class expireStatus {
    public:
        epicsShareFunc expireStatus ( restart_t );
        epicsShareFunc expireStatus ( restart_t, const double & expireDelaySec );
        epicsShareFunc bool restart () const;
        epicsShareFunc double expirationDelay () const;
    private:
        double delay;
    };

    virtual ~epicsTimerNotify () = 0;
    /* return "noRestart" or "expireStatus ( restart, 30.0 )" */
    virtual expireStatus expire ( const epicsTime & currentTime ) = 0;
    virtual void show ( unsigned int level ) const;
};

class epicsShareClass epicsTimer {              /* X aCC 655 */
public:
    /* calls cancel (see warning below) and then destroys the timer */
    virtual void destroy () = 0;
    virtual void start ( epicsTimerNotify &, const epicsTime & ) = 0;
    virtual void start ( epicsTimerNotify &, double delaySeconds ) = 0;
    /* WARNING: A deadlock will occur if you hold a lock while
     * calling this function that you also take within the timer 
     * expiration callback.
     */
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
    virtual ~epicsTimer () = 0; /* protected => delete() must not be called */
};

class epicsTimerQueue {         /* X aCC 655 */
public:
    virtual epicsTimer & createTimer () = 0;
    virtual void show ( unsigned int level ) const = 0;
protected:
    epicsShareFunc virtual ~epicsTimerQueue () = 0;
};

class epicsTimerQueueActive /* X aCC 655 */
    : public epicsTimerQueue {
public:
    static epicsShareFunc epicsTimerQueueActive & allocate (
        bool okToShare, unsigned threadPriority = epicsThreadPriorityMin + 10 );
    virtual void release () = 0; 
protected:
    epicsShareFunc virtual ~epicsTimerQueueActive () = 0;
};

class epicsTimerQueueNotify {   /* X aCC 655 */
public:
    /* called when a new timer is inserted into the queue and the */
    /* delay to the next expire has changed */
    virtual void reschedule () = 0;
    /* if there is a quantum in the scheduling of timer intervals */
    /* return this quantum in seconds. If unknown then return zero. */
    virtual double quantum () = 0;
protected:
    epicsShareFunc virtual ~epicsTimerQueueNotify () = 0;
};

class epicsTimerQueuePassive /* X aCC 655 */
    : public epicsTimerQueue {
public:
    static epicsShareFunc epicsTimerQueuePassive & create ( epicsTimerQueueNotify & );
    epicsShareFunc virtual ~epicsTimerQueuePassive () = 0; /* ok to call delete */
    virtual double process ( const epicsTime & currentTime ) = 0; /* returns delay to next expire */
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
#endif /* __cplusplus */

typedef struct epicsTimerForC * epicsTimerId;
typedef void ( *epicsTimerCallback ) ( void *pPrivate );

/* thread managed timer queue */
typedef struct epicsTimerQueueActiveForC * epicsTimerQueueId;
epicsShareFunc epicsTimerQueueId epicsShareAPI
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueueRelease ( epicsTimerQueueId );
epicsShareFunc epicsTimerId epicsShareAPI 
    epicsTimerQueueCreateTimer ( epicsTimerQueueId queueid, 
        epicsTimerCallback callback, void *arg );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueueDestroyTimer ( epicsTimerQueueId queueid, epicsTimerId id );
epicsShareFunc void  epicsShareAPI 
    epicsTimerQueueShow ( epicsTimerQueueId id, unsigned int level );

/* passive timer queue */
typedef struct epicsTimerQueuePassiveForC * epicsTimerQueuePassiveId;
typedef void ( * epicsTimerQueueNotifyReschedule ) ( void * pPrivate );
typedef double ( * epicsTimerQueueNotifyQuantum ) ( void * pPrivate );
epicsShareFunc epicsTimerQueuePassiveId epicsShareAPI
    epicsTimerQueuePassiveCreate ( epicsTimerQueueNotifyReschedule, 
        epicsTimerQueueNotifyQuantum, void *pPrivate );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId );
epicsShareFunc epicsTimerId epicsShareAPI 
    epicsTimerQueuePassiveCreateTimer (
        epicsTimerQueuePassiveId queueid, epicsTimerCallback pCallback, void *pArg );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueuePassiveDestroyTimer ( epicsTimerQueuePassiveId queueid, epicsTimerId id );
epicsShareFunc double epicsShareAPI 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId );
epicsShareFunc void  epicsShareAPI 
    epicsTimerQueuePassiveShow ( epicsTimerQueuePassiveId id, unsigned int level );

/* timer */
epicsShareFunc void epicsShareAPI 
    epicsTimerStartTime ( epicsTimerId id, const epicsTimeStamp *pTime );
epicsShareFunc void epicsShareAPI 
    epicsTimerStartDelay ( epicsTimerId id, double delaySeconds );
epicsShareFunc void epicsShareAPI 
    epicsTimerCancel ( epicsTimerId id );
epicsShareFunc double epicsShareAPI 
    epicsTimerGetExpireDelay ( epicsTimerId id );
epicsShareFunc void  epicsShareAPI 
    epicsTimerShow ( epicsTimerId id, unsigned int level );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* epicsTimerH */
