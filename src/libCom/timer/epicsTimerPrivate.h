/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#ifndef epicsTimerPrivate_h
#define epicsTimerPrivate_h

#include "tsFreeList.h"
#include "tsDLList.h"
#include "epicsTimer.h"

#ifdef DEBUG
#   define debugPrintf(ARGSINPAREN) printf ARGSINPAREN
#else
#   define debugPrintf(ARGSINPAREN)
#endif

class timer : public epicsTimer, public tsDLNode < timer > {
public:
    timer ( epicsTimerNotify &, class timerQueue & );
    void start ( const epicsTime & );
    void start ( double delaySeconds );
    void cancel ();
    double getExpireDelay () const;
    void show ( unsigned int level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    class noMemory {}; // exception
protected:
    ~timer ();
private:
    enum state { statePending = 45, stateLimbo = 78 };
    epicsTime exp; // experation time 
    state curState; // current state 
    epicsTimerNotify &notify; // callback
    timerQueue &queue;
    void privateStart ( const epicsTime & );
    void privateCancel ();
    double privateDelayToFirstExpire () const;
    static tsFreeList < class timer, 0x20 > freeList;
    friend class timerQueue;
};

class timerQueueNotify {
public:
    virtual void reschedule () = 0;
};

class timerQueue {
public:
    timerQueue ( timerQueueNotify &notify );
    ~timerQueue ();
    void process ();
    double delayToFirstExpire () const;
    void show ( unsigned int level ) const;
private:
    mutable epicsMutex mutex;
    epicsEvent cancelBlockingEvent;
    tsDLList < timer > timerList;
    timerQueueNotify &notify;
    timer *pExpireTmr;
    epicsThreadId processThread;
    bool cancelPending;
    friend class timer;
};

class timerQueueThreadedMgrPrivate {
public:
    timerQueueThreadedMgrPrivate ();
protected:
    virtual ~timerQueueThreadedMgrPrivate () = 0;
private:
    unsigned referenceCount;
    friend class timerQueueThreadedMgr;
};

class timerQueueThreaded : public epicsTimerQueueThreaded, 
    public epicsThreadRunable, public timerQueueNotify,
    public timerQueueThreadedMgrPrivate,
    public tsDLNode < timerQueueThreaded > {
public:
    timerQueueThreaded ( unsigned priority );
    epicsTimer & createTimer ( epicsTimerNotify & );
    void show ( unsigned int level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void release ();
    int threadPriority () const;
protected:
    ~timerQueueThreaded ();
private:
    timerQueue queue;
    epicsEvent rescheduleEvent;
    epicsEvent exitEvent;
    epicsThread thread;
    bool exitFlag;
    bool terminateFlag;
    void run ();
    void reschedule ();
    static tsFreeList < class timerQueueThreaded, 0x8 > freeList;
};

class timerQueueThreadedMgr {
public:
    ~timerQueueThreadedMgr ();
    timerQueueThreaded & create ( bool okToShare, 
        int threadPriority = epicsThreadPriorityMin + 10 );
    void release ( timerQueueThreaded & );
private:
    epicsMutex mutex;
    tsDLList < timerQueueThreaded > queueList;
};

inline void * timer::operator new ( size_t size )
{
    return timer::freeList.allocate ( size );
}

inline void timer::operator delete ( void *pCadaver, size_t size )
{
    timer::freeList.release ( pCadaver, size );
}

inline double timer::privateDelayToFirstExpire () const
{
    if ( this->curState == statePending ) {
        double delay = this->exp - epicsTime::getCurrent ();
        if ( delay < 0.0 ) {
            delay = 0.0;
        }
        return delay;
    }
    else {
        return -DBL_MAX;
    }
}

inline void * timerQueueThreaded::operator new ( size_t size )
{
    return timerQueueThreaded::freeList.allocate ( size );
}

inline void timerQueueThreaded::operator delete ( void *pCadaver, size_t size )
{
    timerQueueThreaded::freeList.release ( pCadaver, size );
}

#endif // epicsTimerPrivate_h

