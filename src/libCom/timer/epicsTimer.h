/* epicsTimer.h */

/* Author:  Jeffrey O. Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
****************************************************************************/

#ifndef epicsTimerH
#define epicsTimerH

#include "epicsAssert.h"
#include "shareLib.h"
#include "epicsTime.h"
#include "epicsThread.h"

#ifdef __cplusplus

class epicsTimerExpireStatus{
public:
    bool again; // Is this a periodic timer? That is reschedule
    double delay; //If again is true the delay until expire is called again
    epicsTimerExpireStatus() : again(false), delay(0.0) {}
    epicsTimerExpireStatus(bool a,double d) : again(a), delay(d) {}
};

// code using a timer must implement epicsTimerNotify
class epicsTimerNotify {
public:
    virtual epicsTimerExpireStatus expire() = 0;
    virtual void show(unsigned int level);
};

class epicsShareClass epicsTimerQueue {
public:
    static epicsTimerQueue &createNonThreaded();	
    static epicsTimerQueue &createThreaded(
        bool okToShare, int threadPriority = threadPriorityMin+10);
    static destroy(epicsTimerQueue &);
    virtual void process() = 0;
    virtual epicsTime getExpireTime() const = 0;
    virtual void show(unsigned int level) const = 0;
};

class epicsShareClass epicsTimer {
public:
    static epicsTimer& create(epicsTimerNotify &,epicsTimerQueue&);
    static destroy(epicsTimer&);
    virtual void start(epicsTime &) = 0;
    virtual void start(double delaySeconds) = 0;
    virtual virtual void cancel() = 0;
    virtual epicsTime getExpireTime() const = 0;
    virtual bool isExpired() const = 0;
    virtual void show(unsigned int level) const = 0;
};

extern "C" {
#endif /*__cplusplus */

typedef void *epicsTimerQueueId;

epicsShareFunc epicsTimerQueueId* epicsShareAPI
    epicsTimerQueueCreateNonThreaded();
epicsShareFunc epicsTimerQueueId* epicsShareAPI
    epicsTimerQueueCreateThreaded(unsigned int threadPriority,int okToShare);
epicsShareFunc void epicsShareAPI epicsTimerQueueDelete(epicsTimerQueueId);

epicsShareFunc void epicsShareAPI epicsTimerQueueProcess(epicsTimerQueueId);
epicsShareFunc int epicsShareAPI epicsTimerQueueGetExpireTime(
    epicsTimerQueueId, epicsTimeStamp *time);
epicsShareFunc void  epicsShareAPI epicsTimerQueueShow(
    epicsTimerQueueId id, unsigned int level);

typedef void *epicsTimerId;
typedef void (*epicsTimerCallback)(void *);

epicsShareFunc epicsTimerId epicsShareAPI epicsTimerCreate(
    epicsTimerQueueId queueid,epicsTimerCallback callback, void *arg);
epicsShareFunc void  epicsShareAPI epicsTimerDestroy(epicsTimerId id);

epicsShareFunc void  epicsShareAPI epicsTimerStartTime(
    epicsTimerId id,epicsTimeStamp *time);
epicsShareFunc void  epicsShareAPI epicsTimerStartDelay(
    epicsTimerId id,double delaySeconds);
epicsShareFunc void  epicsShareAPI epicsTimerCancel(epicsTimerId id);
/* GetExpireTime returns (0,1) if time (is not, is) given a value*/
epicsShareFunc int  epicsShareAPI epicsTimerGetExpireTime(
    epicsTimerId id, epicsTimeStamp *time);
/* epicsTimerIsExpired returns (0,1) if timer (is not, is) expired*/
epicsShareFunc int  epicsShareAPI epicsTimerIsExpired(epicsTimerId id);
epicsShareFunc void  epicsShareAPI epicsTimerShow(
    epicsTimerId id, unsigned int level);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*epicsTimerH*/
