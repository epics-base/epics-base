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
#include "epicsSingleton.h"
#include "tsDLList.h"
#include "epicsTimer.h"

#ifdef DEBUG
#   define debugPrintf(ARGSINPAREN) printf ARGSINPAREN
#else
#   define debugPrintf(ARGSINPAREN)
#endif

template < class T > class epicsGuard;

class timer : public epicsTimer, public tsDLNode < timer > {
public:
    void destroy ();
    void start ( class epicsTimerNotify &, const epicsTime & );
    void start ( class epicsTimerNotify &, double delaySeconds );
    void cancel ();
    expireInfo getExpireInfo () const;
    void show ( unsigned int level ) const;
    class timerQueue & getPrivTimerQueue ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

protected:
    timer ( class timerQueue & );
    ~timer (); 
private:
    enum state { statePending = 45, stateActive = 56, stateLimbo = 78 };
    epicsTime exp; // experation time 
    state curState; // current state 
    epicsTimerNotify * pNotify; // callback
    timerQueue & queue;
    static epicsSingleton < tsFreeList < timer, 0x20 > > pFreeList;
    void privateStart ( epicsTimerNotify & notify, const epicsTime & );
    void privateCancel ( epicsGuard < epicsMutex > & );
    timer & operator = ( const timer & );
    friend class timerQueue;
};

struct epicsTimerForC : public epicsTimerNotify, public timer {
public:
    void destroy ();
protected:
    epicsTimerForC ( timerQueue &, epicsTimerCallback, void *pPrivateIn );
    ~epicsTimerForC (); 
    void * operator new ( size_t size );
    void operator delete (  void *pCadaver, size_t size );
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    static epicsSingleton < tsFreeList < epicsTimerForC, 0x20 > > pFreeList;
    expireStatus expire ( const epicsTime & currentTime );
    epicsTimerForC & operator = ( const epicsTimerForC & );
    friend class timerQueue;
};

class timerQueue : public epicsTimerQueue {
public:
    timerQueue ( epicsTimerQueueNotify &notify );
    virtual ~timerQueue ();
    epicsTimer & createTimer ();
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    double process ( const epicsTime & currentTime );
    void show ( unsigned int level ) const;
private:
    mutable epicsMutex mutex;
    epicsEvent cancelBlockingEvent;
    tsDLList < timer > timerList;
    epicsTimerQueueNotify &notify;
    timer *pExpireTmr;
    epicsThreadId processThread;
    bool cancelPending;
	timerQueue ( const timerQueue & );
    timerQueue & operator = ( const timerQueue & );
    friend class timer;
};

class timerQueueActiveMgrPrivate { // X aCC 655
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
    virtual ~timerQueueActive () = 0;
    epicsTimer & createTimer ();
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    void show ( unsigned int level ) const;
    bool sharingOK () const;
    unsigned threadPriority () const;
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
    epicsTimerQueue & getEpicsTimerQueue ();
	timerQueueActive ( const timerQueueActive & );
    timerQueueActive & operator = ( const timerQueueActive & );
};

class timerQueueActiveMgr {
public:
	timerQueueActiveMgr ();
    ~timerQueueActiveMgr ();
    epicsTimerQueueActiveForC & allocate ( bool okToShare, 
        unsigned threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueActiveForC & );
private:
    epicsMutex mutex;
    tsDLList < epicsTimerQueueActiveForC > sharedQueueList;
	timerQueueActiveMgr ( const timerQueueActiveMgr & );
    timerQueueActiveMgr & operator = ( const timerQueueActiveMgr & );
};

extern epicsSingleton < timerQueueActiveMgr > pTimerQueueMgrEPICS;

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer ();
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    void show ( unsigned int level ) const;
    double process ( const epicsTime & currentTime );
protected:
    timerQueue queue;
    ~timerQueuePassive ();
    epicsTimerQueue & getEpicsTimerQueue ();
	timerQueuePassive ( const timerQueuePassive & );
    timerQueuePassive & operator = ( const timerQueuePassive & );
};

struct epicsTimerQueuePassiveForC : public epicsTimerQueueNotify, public timerQueuePassive {
public:
    epicsTimerQueuePassiveForC ( epicsTimerQueueRescheduleCallback pCallback, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueuePassiveForC ();
private:
    epicsTimerQueueRescheduleCallback pCallback;
    void *pPrivate;
    static epicsSingleton < tsFreeList < epicsTimerQueuePassiveForC, 0x10 > > pFreeList;
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
    static epicsSingleton < tsFreeList < epicsTimerQueueActiveForC, 0x10 > > pFreeList;
	epicsTimerQueueActiveForC ( const epicsTimerQueueActiveForC & );
    epicsTimerQueueActiveForC & operator = ( const epicsTimerQueueActiveForC & );
};

inline bool timerQueueActive::sharingOK () const
{
    return this->okToShare;
}

inline unsigned timerQueueActive::threadPriority () const
{
    return thread.getPriority ();
}

inline void * timer::operator new ( size_t size ) 
{
    return timer::pFreeList->allocate ( size );
}

inline void timer::operator delete ( void *pCadaver, size_t size ) 
{
    timer::pFreeList->release ( pCadaver, size );
}

inline void * epicsTimerForC::operator new ( size_t size ) 
{
    return epicsTimerForC::pFreeList->allocate ( size );
}

inline void epicsTimerForC::operator delete ( void *pCadaver, size_t size ) 
{
    epicsTimerForC::pFreeList->release ( pCadaver, size );
}

inline void * epicsTimerQueuePassiveForC::operator new ( size_t size )
{ 
    return epicsTimerQueuePassiveForC::pFreeList->allocate ( size );
}

inline void epicsTimerQueuePassiveForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerQueuePassiveForC::pFreeList->release ( pCadaver, size );
}

inline void * epicsTimerQueueActiveForC::operator new ( size_t size )
{
    return epicsTimerQueueActiveForC::pFreeList->allocate ( size );
}

inline void epicsTimerQueueActiveForC::operator delete ( void *pCadaver, size_t size )
{
    epicsTimerQueueActiveForC::pFreeList->release ( pCadaver, size );
}

#endif // epicsTimerPrivate_h

