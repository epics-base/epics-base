/* epicsTimer.h */

/* Authors: Marty Kraimer, Jeff Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
****************************************************************************/

#ifndef epicsTimerH
#define epicsTimerH

#include <float.h>

#include "shareLib.h"
#include "epicsAssert.h"
#include "epicsTime.h"
#include "epicsThread.h"

#ifdef __cplusplus

//
// Notes:
// 1) epicsTimer does not hold its lock when calling callbacks.
//

// code using a timer must implement epicsTimerNotify
class epicsTimerNotify {
public:
    enum restart_t { noRestart, restart };
    class expireStatus {
    public:
        expireStatus ( restart_t );
        expireStatus ( restart_t, const double &expireDelaySec );
        bool restart () const;
        double expirationDelay () const;
    private:
        bool again;
        double delay;
    };
    // return noRestart OR return expireStatus ( restart, 30.0 /* sec */ );
    virtual expireStatus expire ( const epicsTime & currentTime ) = 0;
    virtual epicsShareFunc void show ( unsigned int level ) const;
};

class epicsTimer {
public:
    virtual ~epicsTimer () = 0; 
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
    virtual class epicsTimerQueue & getQueue () const = 0;
};

class epicsTimerQueue {
public:
    virtual ~epicsTimerQueue () = 0;
    virtual epicsTimer & createTimer () = 0;
    virtual void destroyTimer ( epicsTimer & ) = 0;
    virtual void show ( unsigned int level ) const = 0;
};

class epicsTimerQueueActive : public epicsTimerQueue {
public:
    virtual ~epicsTimerQueueActive () = 0;
    static epicsShareFunc epicsTimerQueueActive & allocate (
        bool okToShare, int threadPriority = epicsThreadPriorityMin + 10 );
    virtual void release () = 0; 
};

class epicsTimerQueueNotify {
public:
    // called when a new timer is inserted into the queue and the
    // delay to the next expire has changed
    virtual void reschedule () = 0;
};

class epicsTimerQueuePassive : public epicsTimerQueue {
public:
    static epicsShareFunc epicsTimerQueuePassive & create ( epicsTimerQueueNotify & );
    virtual ~epicsTimerQueuePassive () = 0;
    virtual double process ( const epicsTime & currentTime ) = 0; // returns delay to next expire
};

inline epicsTimerNotify::expireStatus::expireStatus ( restart_t restart ) : 
    again ( false ), delay ( - DBL_MAX )
{
    assert ( restart == noRestart );
}

inline epicsTimerNotify::expireStatus::expireStatus 
    ( restart_t, const double &expireDelaySec ) :
    again ( true ), delay ( expireDelaySec )
{
    assert ( this->delay >= 0.0 );
}
    
inline bool epicsTimerNotify::expireStatus::restart () const
{
    return this->again;
}

inline double epicsTimerNotify::expireStatus::expirationDelay () const
{
    assert ( this->again );
    return this->delay;
}

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
    else {
        return - DBL_MAX;
    }
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
typedef void ( *epicsTimerQueueRescheduleCallback ) ( void *pPrivate );
epicsShareFunc epicsTimerQueuePassiveId epicsShareAPI
    epicsTimerQueuePassiveCreate ( epicsTimerQueueRescheduleCallback, void *pPrivate );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId );
epicsShareFunc epicsTimerId epicsShareAPI 
    epicsTimerQueuePassiveCreateTimer (
        epicsTimerQueuePassiveId queueid, epicsTimerCallback pCallback, void *pArg );
epicsShareFunc void epicsShareAPI 
    epicsTimerQueuePassiveDestroyTimer ( epicsTimerQueuePassiveId queueid, epicsTimerId id );
epicsShareFunc double epicsShareAPI 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId );
epicsShareFunc void  epicsShareAPI epicsTimerQueuePassiveShow (
    epicsTimerQueuePassiveId id, unsigned int level );

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
