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
    void start ( class epicsTimerNotify &, const epicsTime & );
    void start ( class epicsTimerNotify &, double delaySeconds );
    void cancel ();
    expireInfo getExpireInfo () const;
    void show ( unsigned int level ) const;
    void destroyTimerForC ( epicsTimerForC & );
protected:
    timer ( class timerQueue & );
    ~timer (); 
private:
    enum state { statePending = 45, stateLimbo = 78 };
    epicsTime exp; // experation time 
    state curState; // current state 
    epicsTimerNotify *pNotify; // callback
    timerQueue &queue;
    void privateStart ( epicsTimerNotify & notify, const epicsTime & );
    void privateCancel ();
    friend class timerQueue;
};

struct epicsTimerForC : public epicsTimerNotify, public timer {
public:
    epicsTimerForC ( timerQueue &, epicsTimerCallback, void *pPrivateIn );
    void destroy ();
protected:
    ~epicsTimerForC (); // intentionally not implemented ( see destroy )
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    expireStatus expire ( const epicsTime & currentTime );
};

class timerQueue {
public:
    timerQueue ( epicsTimerQueueNotify &notify );
    ~timerQueue ();
    double process ( const epicsTime & currentTime );
    void show ( unsigned int level ) const;
    timer & createTimer ();
    void destroyTimer ( timer & );
    epicsTimerForC & createTimerForC ( epicsTimerCallback, void *pPrivateIn );
    void destroyTimerForC ( epicsTimerForC & );
private:
    mutable epicsMutex mutex;
    tsFreeList < class timer, 0x20 > timerFreeList;
    tsFreeList < epicsTimerForC, 0x20 > cTimerfreeList;
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

class timerQueueActive : public epicsTimerQueueActive, 
    public epicsThreadRunable, public epicsTimerQueueNotify,
    public timerQueueActiveMgrPrivate {
public:
    timerQueueActive ( bool okToShare, unsigned priority );
    ~timerQueueActive () = 0;
    epicsTimer & createTimer ();
    void destroyTimer ( epicsTimer & );
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

struct epicsTimerQueueActiveForC : public timerQueueActive, 
    public tsDLNode < epicsTimerQueueActiveForC > {
public:
    epicsTimerQueueActiveForC ( bool okToShare, unsigned priority );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueueActiveForC ();
private:
    static tsFreeList < epicsTimerQueueActiveForC > freeList;
    static epicsMutex freeListMutex;
};

class timerQueueActiveMgr {
public:
    ~timerQueueActiveMgr ();
    epicsTimerQueueActiveForC & allocate ( bool okToShare, 
        int threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueActiveForC & );
private:
    epicsMutex mutex;
    tsDLList < epicsTimerQueueActiveForC > sharedQueueList;
};

extern timerQueueActiveMgr queueMgr;

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer ();
    void destroyTimer ( epicsTimer & );
    double process ( const epicsTime & currentTime );
    void show ( unsigned int level ) const;
    void release ();
    timerQueue & getTimerQueue ();
protected:
    ~timerQueuePassive ();
private:
    timerQueue queue;
};

inline epicsTimerForC & timerQueue::createTimerForC 
    ( epicsTimerCallback pCB, void *pPriv )
{
    epicsAutoMutex autoLock ( this->mutex );
    void *pBuf = this->cTimerfreeList.allocate ( sizeof (epicsTimerForC) );
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    return * new ( pBuf ) epicsTimerForC ( *this, pCB, pPriv );
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

inline timerQueue & timerQueuePassive::getTimerQueue ()
{
    return this->queue;
}

inline void * epicsTimerQueueActiveForC::operator new ( size_t size )
{ 
    epicsAutoMutex locker ( epicsTimerQueueActiveForC::freeListMutex );
    return epicsTimerQueueActiveForC::freeList.allocate ( size );
}

inline void epicsTimerQueueActiveForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsAutoMutex locker ( epicsTimerQueueActiveForC::freeListMutex );
    epicsTimerQueueActiveForC::freeList.release ( pCadaver, size );
}

#endif // epicsTimerPrivate_h

