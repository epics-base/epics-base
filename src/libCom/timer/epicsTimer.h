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

// code using a timer must implement epicsTimerNotify
class epicsShareClass epicsTimerNotify {
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
    virtual expireStatus expire () = 0;
    virtual void show ( unsigned int level ) const;
};

class epicsShareClass epicsTimer {
public:
    virtual ~epicsTimer () = 0;
    virtual void start ( const epicsTime & ) = 0;
    virtual void start ( double delaySeconds ) = 0;
    virtual void cancel () = 0;
    virtual double getExpireDelay () const = 0;
    virtual void show ( unsigned int level ) const = 0;
};

class epicsShareClass epicsTimerQueueThreaded {
public:
    static epicsTimerQueueThreaded & create (
        bool okToShare, int threadPriority = epicsThreadPriorityMin + 10 );
    virtual epicsTimer & createTimer ( epicsTimerNotify & ) = 0;
    virtual void show ( unsigned int level ) const = 0;
    virtual void release () = 0; 
protected:
    virtual ~epicsTimerQueueThreaded () = 0;
};

class epicsShareClass epicsTimerQueueNonThreaded {
public:
    static epicsTimerQueueNonThreaded & create ();
    virtual epicsTimer & createTimer ( epicsTimerNotify & ) = 0;
    virtual void process () = 0;
    virtual double getNextExpireDelay () const = 0;
    virtual void show ( unsigned int level ) const = 0;
    virtual void release () = 0;
protected:
    virtual ~epicsTimerQueueNonThreaded () = 0;
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


extern "C" {
#endif /* __cplusplus */

typedef void *epicsTimerQueueId;

epicsShareFunc epicsTimerQueueId* epicsShareAPI
    epicsTimerQueueCreateNonThreaded ();
epicsShareFunc epicsTimerQueueId* epicsShareAPI
    epicsTimerQueueCreateThreaded ( unsigned int threadPriority, int okToShare );
epicsShareFunc void epicsShareAPI epicsTimerQueueDelete ( epicsTimerQueueId );

epicsShareFunc void epicsShareAPI epicsTimerQueueProcess ( epicsTimerQueueId );
epicsShareFunc int epicsShareAPI epicsTimerQueueGetExpireTime (
    epicsTimerQueueId, epicsTimeStamp *time );
epicsShareFunc void  epicsShareAPI epicsTimerQueueShow (
    epicsTimerQueueId id, unsigned int level );

typedef void *epicsTimerId;
typedef void ( *epicsTimerCallback ) ( void * );

epicsShareFunc epicsTimerId epicsShareAPI epicsTimerCreate (
    epicsTimerQueueId queueid,epicsTimerCallback callback, void *arg );
epicsShareFunc void  epicsShareAPI epicsTimerDestroy ( epicsTimerId id );

epicsShareFunc void  epicsShareAPI epicsTimerStartTime (
    epicsTimerId id,epicsTimeStamp *time );
epicsShareFunc void  epicsShareAPI epicsTimerStartDelay (
    epicsTimerId id,double delaySeconds );
epicsShareFunc void  epicsShareAPI epicsTimerCancel ( epicsTimerId id );
/* GetExpireTime returns (0,1) if time (is not, is) given a value */
epicsShareFunc int  epicsShareAPI epicsTimerGetExpireTime (
    epicsTimerId id, epicsTimeStamp *time);
epicsShareFunc void  epicsShareAPI epicsTimerShow (
    epicsTimerId id, unsigned int level );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* epicsTimerH */
