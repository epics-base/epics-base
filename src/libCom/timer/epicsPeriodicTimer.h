/* epicsPeriodicTimer.h */

/* Author:  Jeffrey O. Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
****************************************************************************/

#ifndef epicsPeriodicTimerH
#define epicsPeriodicTimerH

#include "epicsTimer.h"

#ifdef __cplusplus

class epicsShareClass epicsPeriodicTimer : public epicsTimer {
public:
    epicsPeriodicTimer(epicsTimerNotify &,double period,
        epicsTimerQueue&);
    epicsPeriodicTimer(epicsTimerNotify &,double period,
        sharedPriority priority = priorityLow);
    virtual ~epicsPeriodicTimer();
    void setPeriod(double seconds);
    void start();
    void stop();
    double getPeriod() const;
    virtual void show(unsigned int level) const;
private:
    class periodicImpl &periodicPvt;
private: //copy constructor and operator= not allowed
    epicsPeriodicTimer(const epicsPeriodicTimer& rhs);
    epicsPeriodicTimer& 
        operator=(const epicsPeriodicTimer& rhs);
};

extern "C" {
#endif /* __cplusplus */

typedef void *epicsPeriodicTimerId;

epicsShareFunc epicsPeriodicTimerId epicsShareAPI
epicsPeriodicTimerCreate(
    epicsTimerQueueId queueid,double period,
    epicsTimerCallback cb,void *arg);
epicsShareFunc epicsPeriodicTimerId epicsShareAPI
epicsPeriodicTimerCreateShared(
    epicsTimerSharedPriority priority, double period,
    epicsTimerCallback cb,void *arg);
epicsShareFunc void epicsShareAPI
epicsPeriodicTimerDestroy(epicsPeriodicTimerId id);
epicsShareFunc void epicsShareAPI
epicsPeriodicTimerSetPeriod(
    epicsPeriodicTimerId id,double seconds);
epicsShareFunc void epicsShareAPI
    epicsPeriodicTimerStart(epicsPeriodicTimerId id);
epicsShareFunc void epicsShareAPI
    epicsPeriodicTimerStop(epicsPeriodicTimerId id);
epicsShareFunc double epicsShareAPI
    epicsPeriodicTimerGetPeriod(epicsPeriodicTimerId id);
/* GetExpireTime returns (0,1) if time (is not, is) given a value*/
epicsShareFunc int epicsShareAPI
epicsPeriodicTimerGetExpireTime(
    epicsPeriodicTimerId id, epicsTimeStamp *time);
epicsShareFunc void epicsShareAPI
epicsPeriodicTimerShow(epicsPeriodicTimerId id,unsigned int level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*epicsPeriodicTimerH*/
