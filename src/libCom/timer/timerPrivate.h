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

class timerQueue {
public:
    timerQueue ( epicsTimerQueueNotify &notify );
    ~timerQueue ();
    void process ();
    double delayToFirstExpire () const;
    void show ( unsigned int level ) const;
private:
    mutable epicsMutex mutex;
    epicsEvent cancelBlockingEvent;
    tsDLList < timer > timerList;
    epicsTimerQueueNotify &notify;
    timer *pExpireTmr;
    epicsThreadId processThread;
    bool cancelPending;
    friend class timer;
};

class timerQueueActiveMgrPrivate {
public:
    timerQueueActiveMgrPrivate ();
protected:
    virtual ~timerQueueActiveMgrPrivate () = 0;
private:
    unsigned referenceCount;
    friend class timerQueueActiveMgr;
};

class timerQueueActive : public epicsTimerQueue, 
    public epicsThreadRunable, public epicsTimerQueueNotify,
    public timerQueueActiveMgrPrivate {
public:
    timerQueueActive ( bool okToShare, unsigned priority );
    ~timerQueueActive () = 0;
    epicsTimer & createTimer ( epicsTimerNotify & );
    void show ( unsigned int level ) const;
    bool sharingOK () const;
    int threadPriority () const;
    timerQueue & getTimerQueue ();
private:
    timerQueue queue;
    epicsEvent rescheduleEvent;
    epicsEvent exitEvent;
    epicsThread thread;
    bool okToShare;
    bool exitFlag;
    bool terminateFlag;
    void run ();
    void reschedule ();
};

struct epicsTimerQueueForC : public timerQueueActive, 
    public tsDLNode < epicsTimerQueueForC > {
public:
    epicsTimerQueueForC ( bool okToShare, unsigned priority );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueueForC ();
private:
    static tsFreeList < epicsTimerQueueForC > freeList;
};

class timerQueueActiveMgr {
public:
    ~timerQueueActiveMgr ();
    epicsTimerQueueForC & allocate ( bool okToShare, 
        int threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueForC & );
private:
    epicsMutex mutex;
    tsDLList < epicsTimerQueueForC > sharedQueueList;
};

extern timerQueueActiveMgr queueMgr;

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer ( epicsTimerNotify & );
    void process ();
    double getNextExpireDelay () const;
    void show ( unsigned int level ) const;
    void release ();
    timerQueue & getTimerQueue ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~timerQueuePassive ();
private:
    timerQueue queue;
    static tsFreeList < class timerQueuePassive, 0x8 > freeList;
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

inline bool timerQueueActive::sharingOK () const
{
    return this->okToShare;
}

inline int timerQueueActive::threadPriority () const
{
    return thread.getPriority ();
}

inline timerQueue & timerQueueActive::getTimerQueue ()
{
    return this->queue;
}

inline void * timerQueuePassive::operator new ( size_t size )
{
    return timerQueuePassive::freeList.allocate ( size );
}

inline void timerQueuePassive::operator delete ( void *pCadaver, size_t size )
{
    timerQueuePassive::freeList.release ( pCadaver, size );
}

inline timerQueue & timerQueuePassive::getTimerQueue ()
{
    return this->queue;
}

inline void * epicsTimerQueueForC::operator new ( size_t size )
{ 
    return epicsTimerQueueForC::freeList.allocate ( size );
}

inline void epicsTimerQueueForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerQueueForC::freeList.release ( pCadaver, size );
}

#endif // epicsTimerPrivate_h

