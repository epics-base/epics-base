/* epicsTimerQueue.h */

/* Author:  Jeffrey O. Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
****************************************************************************/

#ifndef epicsTimerQueueH
#define epicsTimerQueueH

#include "epicsTimer.h"

#ifdef __cplusplus

#include "epicsTimer.h"

class epicsTimerQueue {
public:
    epicsTimerQueue()
    virtual ~epicsTimerQueue() = 0;
    epicsTime &getExpirationTime() const;
    double getExpirationDelay() const;
    virtual void show(unsigned int level) const;
protected:
    class impl &queuePvt;
    friend class epicsTimer;
private:
    //copy constructor and operator= not allowed
    epicsTimerQueue(const epicsTimerQueue& rhs);
    epicsTimerQueue& operator=(const epicsTimerQueue& rhs);
};

class epicsTimerQueueThreaded : public epicsTimerQueue {
public:
    epicsTimerQueueThreaded(
        int threadPriority = threadPriorityMin+10);
    virtual ~epicsTimerQueueThreaded();
    int getPriority();
    virtual void show(int level) const;
private:
    class threadedImpl& threadPvt;
    friend class epicsTimer;
    //copy constructor and operator= not allowed
    epicsTimerThreaded(const epicsTimerThreaded& rhs);
    epicsTimerThreaded& operator=
        (const epicsTimerThreaded& rhs);
};

class epicsTimerQueueNonthreaded : public epicsTimerQueue{
public:
    epicsTimerQueueNonthreaded();
    virtual ~epicsTimerQueue();
    void process();
    virtual void show(int level) const;
private:
    class nothreadImpl& nothreadPvt;
    friend class epicsTimer;
    //copy constructor and operator= not allowed
    epicsTimerQueueNonthreaded(
        const epicsTimerQueueNonthreaded& rhs);
    epicsTimerQueueNonthreaded& 
        operator=(const epicsTimerQueueNonthreaded& rhs);
};

#endif /* __cplusplus */

#endif /*epicsTimerQueueH*/
