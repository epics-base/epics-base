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

typedef enum {
    epicsTimerPriorityLow,epicsTimerPriorityMedium,epicsTimerPriorityHigh
}epicsTimerSharedPriority;

#ifdef __cplusplus

// code using a timer must implement epicsTimerNotify
class epicsTimerNotify {
public:
    virtual void expire() = 0;
};

class epicsShareClass epicsTimer {
public:
    epicsTimer(epicsTimerNotify &,epicsTimerQueue&);
    epicsTimer(epicsTimerNotify &,
        epicsTimerSharedPriority priority = epicsTimerPriorityLow);
    virtual ~epicsTimer();
    void start(epicsTime &);
    void start(double delaySeconds);
    virtual void cancel();
    epicsTime getExpireTime() const;
    double getExpireSeconds() const;
    bool isExpired() const;
    epicsTimerNotify &getNotify() const;
    epicsTimerQueue &getQueue() const;
    virtual void show(unsigned int level) const;
    static epicsTimerQueue &createTimerQueue(
        unsigned int threadPriority = epicsThreadPriorityMin);
    static void deleteTimerQueue(epicsTimerQueue &);
protected:
    class impl& timerPvt;
private: //copy constructor and operator= not allowed
    epicsTimer(const epicsTimer&);
    epicsTimer& operator=(const epicsTimer&);
};

extern "C" {
#endif /*__cplusplus */

typedef void *epicsTimerId;
typedef void *epicsTimerQueueId;

typedef void (*epicsTimerCallback)(void *);

epicsShareFunc epicsTimerId epicsShareAPI epicsTimerCreate(
    epicsTimerQueueId queueid,epicsTimerCallback callback, void *arg);
epicsShareFunc epicsTimerId epicsShareAPI epicsTimerCreateShared(
    epicsTimerSharedPriority priority,epicsTimerCallback callback, void *arg);
epicsShareFunc void  epicsShareAPI epicsTimerDestroy(epicsTimerQueueId id);
epicsShareFunc void  epicsShareAPI epicsTimerStartTime(
    epicsTimerQueueId id,epicsTimeStamp *time);
epicsShareFunc void  epicsShareAPI epicsTimerStartDelay(
    epicsTimerQueueId id,double delaySeconds);
epicsShareFunc void  epicsShareAPI epicsTimerCancel(epicsTimerQueueId id);
/* GetExpireTime returns (0,1) if time (is not, is) given a value*/
epicsShareFunc int  epicsShareAPI epicsTimerGetExpireTime(
    epicsTimerQueueId id, epicsTimeStamp *time);
epicsShareFunc int  epicsShareAPI epicsTimerIsExpired(epicsTimerQueueId id);
epicsShareFunc void  epicsShareAPI epicsTimerShow(
    epicsTimerQueueId id, unsigned int level);

epicsShareFunc epicsTimerQueueId epicsShareAPI
epicsTimerQueueCreate(unsigned int threadPriority);

epicsShareFunc void epicsShareAPI epicsTimerQueueDelete(epicsTimerQueueId);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*epicsTimerH*/
