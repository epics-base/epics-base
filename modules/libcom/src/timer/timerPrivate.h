/*************************************************************************\
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef epicsTimerPrivate_h
#define epicsTimerPrivate_h

#include <typeinfo>

#include "tsFreeList.h"
#include "epicsSingleton.h"
#include "tsDLList.h"
#include "epicsTimer.h"
#include "compilerDependencies.h"

#if __cplusplus<201103L
#  define final
#  define override
#endif

#ifdef DEBUG
#   define debugPrintf(ARGSINPAREN) printf ARGSINPAREN
#else
#   define debugPrintf(ARGSINPAREN)
#endif

template < class T > class epicsGuard;

class timer : public epicsTimer, public tsDLNode < timer > {
public:
    void destroy () override;
    void start ( class epicsTimerNotify &, const epicsTime & ) override final;
    void start ( class epicsTimerNotify &, double delaySeconds ) override final;
    void cancel () override final;
    expireInfo getExpireInfo () const override final;
    void show ( unsigned int level ) const override final;
    void * operator new ( size_t size, tsFreeList < timer, 0x20 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < timer, 0x20 > & ))
protected:
    timer ( class timerQueue & );
    ~timer ();
    timerQueue & queue;
private:
    enum state { statePending = 45, stateActive = 56, stateLimbo = 78 };
    epicsTime exp; // expiration time
    state curState; // current state
    epicsTimerNotify * pNotify; // callback
    void privateStart ( epicsTimerNotify & notify, const epicsTime & );
    timer & operator = ( const timer & );
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    void operator delete ( void * );
    friend class timerQueue;
};

struct epicsTimerForC final : public epicsTimerNotify, public timer {
public:
    void destroy () override final;
protected:
    epicsTimerForC ( timerQueue &, epicsTimerCallback, void *pPrivateIn );
    ~epicsTimerForC ();
    void * operator new ( size_t size, tsFreeList < epicsTimerForC, 0x20 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < epicsTimerForC, 0x20 > & ))
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    expireStatus expire ( const epicsTime & currentTime ) override final;
    epicsTimerForC & operator = ( const epicsTimerForC & );
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    void operator delete ( void * );
    friend class timerQueue;
};

using std :: type_info;

class timerQueue : public epicsTimerQueue {
public:
    timerQueue ( epicsTimerQueueNotify &notify );
    virtual ~timerQueue ();
    epicsTimer & createTimer () override final;
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    double process ( const epicsTime & currentTime );
    void show ( unsigned int level ) const override final;
private:
    tsFreeList < timer, 0x20 > timerFreeList;
    tsFreeList < epicsTimerForC, 0x20 > timerForCFreeList;
    mutable epicsMutex mutex;
    epicsEvent cancelBlockingEvent;
    tsDLList < timer > timerList;
    epicsTimerQueueNotify & notify;
    timer * pExpireTmr;
    epicsThreadId processThread;
    epicsTime exceptMsgTimeStamp;
    bool cancelPending;
    static const double exceptMsgMinPeriod;
    void printExceptMsg ( const char * pName,
                const type_info & type );
    timerQueue ( const timerQueue & );
    timerQueue & operator = ( const timerQueue & );
    friend class timer;
    friend struct epicsTimerForC;
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

class timerQueueActiveMgr;

class timerQueueActive : public epicsTimerQueueActive,
    public epicsThreadRunable, public epicsTimerQueueNotify,
    public timerQueueActiveMgrPrivate {
public:
    typedef epicsSingleton < timerQueueActiveMgr > :: reference RefMgr;
    timerQueueActive ( RefMgr &, bool okToShare, unsigned priority );
    void start ();
    epicsTimer & createTimer () override final;
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    void show ( unsigned int level ) const override final;
    bool sharingOK () const;
    unsigned threadPriority () const;
protected:
    ~timerQueueActive ();
    RefMgr _refMgr;
private:
    timerQueue queue;
    epicsEvent rescheduleEvent;
    epicsEvent exitEvent;
    epicsThread thread;
    const double sleepQuantum;
    bool okToShare;
    int exitFlag; // use atomic ops
    bool terminateFlag;
    void run () override final;
    void reschedule () override final;
    double quantum () override final;
    void _printLastChanceExceptionMessage (
                const char * pExceptionTypeName,
                const char * pExceptionContext );
    epicsTimerQueue & getEpicsTimerQueue ();
    timerQueueActive ( const timerQueueActive & );
    timerQueueActive & operator = ( const timerQueueActive & );
};

class timerQueueActiveMgr final {
public:
    typedef epicsSingleton < timerQueueActiveMgr > :: reference RefThis;
    timerQueueActiveMgr ();
    ~timerQueueActiveMgr ();
    epicsTimerQueueActiveForC & allocate ( RefThis &, bool okToShare,
        unsigned threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueActiveForC & );
private:
    epicsMutex mutex;
    tsDLList < epicsTimerQueueActiveForC > sharedQueueList;
    timerQueueActiveMgr ( const timerQueueActiveMgr & );
    timerQueueActiveMgr & operator = ( const timerQueueActiveMgr & );
};

extern epicsSingleton < timerQueueActiveMgr > timerQueueMgrEPICS;

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer () override final;
    epicsTimerForC & createTimerForC ( epicsTimerCallback pCallback, void *pArg );
    void show ( unsigned int level ) const override final;
    double process ( const epicsTime & currentTime ) override final;
protected:
    timerQueue queue;
    ~timerQueuePassive ();
    epicsTimerQueue & getEpicsTimerQueue ();
    timerQueuePassive ( const timerQueuePassive & );
    timerQueuePassive & operator = ( const timerQueuePassive & );
};

struct epicsTimerQueuePassiveForC final :
    public epicsTimerQueueNotify, public timerQueuePassive {
public:
    epicsTimerQueuePassiveForC (
        epicsTimerQueueNotifyReschedule,
        epicsTimerQueueNotifyQuantum,
        void * pPrivate );
    void destroy ();
protected:
    ~epicsTimerQueuePassiveForC ();
private:
    epicsTimerQueueNotifyReschedule pRescheduleCallback;
    epicsTimerQueueNotifyQuantum pSleepQuantumCallback;
    void * pPrivate;
    static epicsSingleton < tsFreeList < epicsTimerQueuePassiveForC, 0x10 > > pFreeList;
    void reschedule () override final;
    double quantum () override final;
};

struct epicsTimerQueueActiveForC final : public timerQueueActive,
    public tsDLNode < epicsTimerQueueActiveForC > {
public:
    epicsTimerQueueActiveForC ( RefMgr &, bool okToShare, unsigned priority );
    void release () override final;
    void * operator new ( size_t );
    void operator delete ( void * );
protected:
    virtual ~epicsTimerQueueActiveForC ();
private:
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

inline void * timer::operator new ( size_t size,
                     tsFreeList < timer, 0x20 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void timer::operator delete ( void * pCadaver,
                     tsFreeList < timer, 0x20 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline void * epicsTimerForC::operator new ( size_t size,
                        tsFreeList < epicsTimerForC, 0x20 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void epicsTimerForC::operator delete ( void * pCadaver,
                        tsFreeList < epicsTimerForC, 0x20 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline void * epicsTimerQueueActiveForC::operator new ( size_t size )
{
    return ::operator new ( size );
}

inline void epicsTimerQueueActiveForC::operator delete ( void * pCadaver )
{
    ::operator delete ( pCadaver );
}

#endif // epicsTimerPrivate_h

