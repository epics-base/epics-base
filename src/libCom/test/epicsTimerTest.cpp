/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "epicsTimer.h"
#include "epicsEvent.h"
#include "epicsAssert.h"
#include "epicsGuard.h"
#include "tsFreeList.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

class notified : public epicsTimerNotify
{
public:
    bool done;
    notified() : epicsTimerNotify(), done(false) {}

    expireStatus expire(const epicsTime &currentTime)
    {done=true; return expireStatus(noRestart);}
};

void testRefCount()
{
    notified action;

    epicsTimerQueueActive *Q1, *Q2;
    epicsTimer *T1, *T2;

    Q1 = &epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMin );

    T1 = &Q1->createTimer();
    //timer->start(action, 0.0);

    Q2 = &epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMin );

    testOk1(Q1==Q2);

    T2 = &Q2->createTimer();

    T2->destroy();
    Q2->release();

    T1->destroy();
    Q1->release();
}

static const double delayVerifyOffset = 1.0; // sec 

class delayVerify : public epicsTimerNotify {
public:
    delayVerify ( double expectedDelay, epicsTimerQueue & );
    void start ( const epicsTime &expireTime );
    void setBegin ( const epicsTime & );
    double delay () const;
    double checkError () const;
protected:
    virtual ~delayVerify ();
private:
    epicsTimer &timer;
    epicsTime beginStamp;
    epicsTime expireStamp;
    double expectedDelay;
    expireStatus expire ( const epicsTime & );
    delayVerify ( const delayVerify & );
    delayVerify & operator = ( const delayVerify & );
};

static volatile unsigned expireCount;
static epicsEvent expireEvent;

delayVerify::delayVerify ( double expectedDelayIn, epicsTimerQueue &queueIn ) :
    timer ( queueIn.createTimer() ), expectedDelay ( expectedDelayIn )
{
}

delayVerify::~delayVerify ()
{
    this->timer.destroy ();
}

inline void delayVerify::setBegin ( const epicsTime &beginIn )
{
    this->beginStamp = beginIn;
}

inline double delayVerify::delay () const
{
    return this->expectedDelay;
}

double delayVerify::checkError () const
{
    const double messageThresh = 2.0; // percent 
    double actualDelay =  this->expireStamp - this->beginStamp;
    double measuredError = actualDelay - this->expectedDelay;
    double percentError = 100.0 * fabs ( measuredError ) / this->expectedDelay;
    if ( ! testOk1 ( percentError < messageThresh ) ) {
        testDiag ( "delay = %f s, error = %f s (%.1f %%)", 
            this->expectedDelay, measuredError, percentError );
    }
    return measuredError;
}

inline void delayVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( *this, expireTime );
}

epicsTimerNotify::expireStatus delayVerify::expire ( const epicsTime &currentTime )
{
    this->expireStamp = currentTime;
    if ( --expireCount == 0u ) {
        expireEvent.signal ();
    }
    return noRestart;
}

//
// verify reasonable timer interval accuracy
//
void testAccuracy ()
{
    static const unsigned nTimers = 25u;
    delayVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;

    testDiag ( "Testing timer accuracy" );

    epicsTimerQueueActive &queue = 
        epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMax );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new delayVerify ( i * 0.1 + delayVerifyOffset, queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );

    expireCount = nTimers;
    for ( i = 0u; i < nTimers; i++ ) {
        epicsTime cur = epicsTime::getCurrent ();
        pTimers[i]->setBegin ( cur );
        pTimers[i]->start ( cur + pTimers[i]->delay () );
    }
    while ( expireCount != 0u ) {
        expireEvent.wait ();
    }
    double averageMeasuredError = 0.0;
    for ( i = 0u; i < nTimers; i++ ) {
        averageMeasuredError += pTimers[i]->checkError ();
    }
    averageMeasuredError /= nTimers;
    testDiag ("average timer delay error %f ms", 
        averageMeasuredError * 1000 );
    queue.release ();
}


class cancelVerify : public epicsTimerNotify {
public:
    cancelVerify ( epicsTimerQueue & );
    void start ( const epicsTime &expireTime );
    void cancel ();
    static unsigned cancelCount;
    static unsigned expireCount;
protected:
    virtual ~cancelVerify ();
private:
    epicsTimer &timer;
    expireStatus expire ( const epicsTime & );
    cancelVerify ( const cancelVerify & );
    cancelVerify & operator = ( const cancelVerify & );
};

unsigned cancelVerify::cancelCount;
unsigned cancelVerify::expireCount;

cancelVerify::cancelVerify ( epicsTimerQueue &queueIn ) :
    timer ( queueIn.createTimer () )
{
}

cancelVerify::~cancelVerify ()
{
    this->timer.destroy ();
}

inline void cancelVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( *this, expireTime );
}

inline void cancelVerify::cancel ()
{
    this->timer.cancel ();
    ++cancelVerify::cancelCount;
}

epicsTimerNotify::expireStatus cancelVerify::expire ( const epicsTime & )
{
    ++cancelVerify::expireCount;
    double root = 3.14159;
    for ( unsigned i = 0u; i < 1000; i++ ) {
        root = sqrt ( root );
    }
    return noRestart;
}

//
// verify that expire() won't be called after the timer is cancelled
//
void testCancel ()
{
    static const unsigned nTimers = 25u;
    cancelVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;

    cancelVerify::cancelCount = 0;
    cancelVerify::expireCount = 0;

    testDiag ( "Testing timer cancellation" );

    epicsTimerQueueActive &queue = 
        epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new cancelVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );
    if ( ! testOk1 ( cancelVerify::expireCount == 0 ) )
        testDiag ( "expireCount = %u", cancelVerify::expireCount );
    if ( ! testOk1 ( cancelVerify::cancelCount == 0 ) )
        testDiag ( "cancelCount = %u", cancelVerify::cancelCount );

    testDiag ( "starting %d timers", nTimers );
    epicsTime exp = epicsTime::getCurrent () + 4.0;
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( exp );
    }
    testOk1 ( cancelVerify::expireCount == 0 );
    testOk1 ( cancelVerify::cancelCount == 0 );

    testDiag ( "cancelling timers" );
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->cancel ();
    }
    testOk1 ( cancelVerify::expireCount == 0 );
    testOk1 ( cancelVerify::cancelCount == nTimers );

    testDiag ( "waiting until after timers should have expired" );
    epicsThreadSleep ( 5.0 );
    testOk1 ( cancelVerify::expireCount == 0 );
    testOk1 ( cancelVerify::cancelCount == nTimers );

    epicsThreadSleep ( 1.0 );
    queue.release ();
}


class expireDestroyVerify : public epicsTimerNotify {
public:
    expireDestroyVerify ( epicsTimerQueue & );
    void start ( const epicsTime &expireTime );
    static unsigned destroyCount;
protected:
    virtual ~expireDestroyVerify ();
private:
    epicsTimer & timer;
    expireStatus expire ( const epicsTime & );
    expireDestroyVerify ( const expireDestroyVerify & );
    expireDestroyVerify & operator = ( const expireDestroyVerify & );
};

unsigned expireDestroyVerify::destroyCount;

expireDestroyVerify::expireDestroyVerify ( epicsTimerQueue & queueIn ) :
    timer ( queueIn.createTimer () )
{
}

expireDestroyVerify::~expireDestroyVerify ()
{
    this->timer.destroy ();
    ++expireDestroyVerify::destroyCount;
}

inline void expireDestroyVerify::start ( const epicsTime & expireTime )
{
    this->timer.start ( *this, expireTime );
}

epicsTimerNotify::expireStatus expireDestroyVerify::expire ( const epicsTime & )
{
    delete this;
    return noRestart;
}

//
// verify that a timer can be destroyed in expire
//
void testExpireDestroy ()
{
    static const unsigned nTimers = 25u;
    expireDestroyVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;
    expireDestroyVerify::destroyCount = 0;

    testDiag ( "Testing timer destruction in expire()" );

    epicsTimerQueueActive &queue = 
        epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new expireDestroyVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );
    testOk1 ( expireDestroyVerify::destroyCount == 0 );

    testDiag ( "starting %d timers", nTimers );
    epicsTime cur = epicsTime::getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur );
    }

    testDiag ( "waiting until all timers should have expired" );
    epicsThreadSleep ( 5.0 );

    testOk1 ( expireDestroyVerify::destroyCount == nTimers );
    queue.release ();
}


class periodicVerify : public epicsTimerNotify {
public:
    periodicVerify ( epicsTimerQueue & );
    void start ( const epicsTime &expireTime );
    void cancel ();
    bool verifyCount ();
protected:
    virtual ~periodicVerify ();
private:
    epicsTimer &timer;
    unsigned nExpire;
    bool cancelCalled;
    expireStatus expire ( const epicsTime & );
    periodicVerify ( const periodicVerify & );
    periodicVerify & operator = ( const periodicVerify & );
};

periodicVerify::periodicVerify ( epicsTimerQueue & queueIn ) :
    timer ( queueIn.createTimer () ), nExpire ( 0u ), 
        cancelCalled ( false )
{
}

periodicVerify::~periodicVerify ()
{
    this->timer.destroy ();
}

inline void periodicVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( *this, expireTime );
}

inline void periodicVerify::cancel ()
{
    this->timer.cancel ();
    this->cancelCalled = true;
}

inline bool periodicVerify::verifyCount ()
{
    return ( this->nExpire > 1u );
}

epicsTimerNotify::expireStatus periodicVerify::expire ( const epicsTime & )
{
    this->nExpire++;
    double root = 3.14159;
    for ( unsigned i = 0u; i < 1000; i++ ) {
        root = sqrt ( root );
    }
    verify ( ! this->cancelCalled );
    double delay = rand ();
    delay = delay / RAND_MAX;
    delay /= 10.0;
    return expireStatus ( restart, delay );
}

//
// verify periodic timers
//
void testPeriodic ()
{
    static const unsigned nTimers = 25u;
    periodicVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;

    testDiag ( "Testing periodic timers" );

    epicsTimerQueueActive &queue = 
        epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new periodicVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );

    testDiag ( "starting %d timers", nTimers );
    epicsTime cur = epicsTime::getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur );
    }

    testDiag ( "waiting until all timers should have expired" );
    epicsThreadSleep ( 5.0 );

    bool notWorking = false;
    for ( i = 0u; i < nTimers; i++ ) {
        notWorking |= ! pTimers[i]->verifyCount ();
        pTimers[i]->cancel ();
    }
    testOk( ! notWorking, "All timers expiring" );
    epicsThreadSleep ( 1.0 );
    queue.release ();
}

MAIN(epicsTimerTest)
{
    testPlan(41);
    testRefCount();
    testAccuracy ();
    testCancel ();
    testExpireDestroy ();
    testPeriodic ();
    return testDone();
}
