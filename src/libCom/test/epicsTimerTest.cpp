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

#include <math.h>

#include "epicsTimer.h"
#include "epicsEvent.h"
#include "epicsAssert.h"
#include "tsFreeList.h"

class delayVerify : public epicsTimerNotify {
public:
    delayVerify ( double expectedDelay, epicsTimerQueue & );
    expireStatus expire ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void start ( const epicsTime &expireTime );
    void setBegin ( const epicsTime & );
    double delay () const;
    void checkError () const;
protected:
    virtual ~delayVerify ();
private:
    epicsTimer &timer;
    epicsTime beginStamp;
    epicsTime expireStamp;
    double expectedDelay;
    static tsFreeList < class delayVerify, 0x20 > freeList;
};

static unsigned expireCount;
static epicsEvent expireEvent;

delayVerify::delayVerify ( double expectedDelayIn, epicsTimerQueue &queue ) :
    timer ( queue.createTimer ( *this ) ), expectedDelay ( expectedDelayIn )
{
}

delayVerify::~delayVerify ()
{
}

inline void delayVerify::setBegin ( const epicsTime &beginIn )
{
    this->beginStamp = beginIn;
}

inline double delayVerify::delay () const
{
    return this->expectedDelay;
}

void delayVerify::checkError () const
{
    double actualDelay =  this->expireStamp - this->beginStamp;
    double measuredError = actualDelay - this->expectedDelay;
    double percentError = measuredError / this->expectedDelay;
    percentError *= 100.0;
    if ( percentError > 1.0 ) {
        printf ( "TEST FAILED timer delay = %g sec, error = %g mSec (%f %%)\n", 
            this->expectedDelay, measuredError * 1000.0, percentError );
    }
}

inline void delayVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( expireTime );
}

tsFreeList < class delayVerify, 0x20 > delayVerify::freeList;

inline void * delayVerify::operator new ( size_t size )
{
    return delayVerify::freeList.allocate ( size );
}

inline void delayVerify::operator delete ( void *pCadaver, size_t size )
{
    delayVerify::freeList.release ( pCadaver, size );
}

epicsTimerNotify::expireStatus delayVerify::expire ()
{
    this->expireStamp = epicsTime::getCurrent ();
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

    epicsTimerQueue &queue = epicsTimerQueue::allocate ( true, epicsThreadPriorityMax );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new delayVerify ( ( nTimers - i ) * 0.1, queue );
        assert ( pTimers[i] );
    }
    expireCount = nTimers;
    for ( i = 0u; i < nTimers; i++ ) {
        epicsTime cur = epicsTime::getCurrent ();
        pTimers[i]->setBegin ( cur );
        pTimers[i]->start ( cur + pTimers[i]->delay () );
    }
    while ( expireCount != 0u ) {
        expireEvent.wait ();
    }
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->checkError ();
    }
    queue.release ();
}

class cancelVerify : public epicsTimerNotify {
public:
    cancelVerify ( epicsTimerQueue & );
    expireStatus expire ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void start ( const epicsTime &expireTime );
    void cancel ();
protected:
    virtual ~cancelVerify ();
private:
    epicsTimer &timer;
    bool failOutIfExpireIsCalled;
    static tsFreeList < class cancelVerify, 0x20 > freeList;
};

cancelVerify::cancelVerify ( epicsTimerQueue &queue ) :
    timer ( queue.createTimer ( *this ) ), failOutIfExpireIsCalled ( false )
{
}

cancelVerify::~cancelVerify ()
{
}

inline void cancelVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( expireTime );
}

tsFreeList < class cancelVerify, 0x20 > cancelVerify::freeList;

inline void * cancelVerify::operator new ( size_t size )
{
    return cancelVerify::freeList.allocate ( size );
}

inline void cancelVerify::operator delete ( void *pCadaver, size_t size )
{
    cancelVerify::freeList.release ( pCadaver, size );
}

inline void cancelVerify::cancel ()
{
    this->timer.cancel ();
    this->failOutIfExpireIsCalled = true;
}

epicsTimerNotify::expireStatus cancelVerify::expire ()
{
    double root = 3.14159;
    for ( unsigned i = 0u; i < 10000; i++ ) {
        root = sqrt ( root );
    }
    assert ( ! this->failOutIfExpireIsCalled );
    return noRestart;
}

//
// verify that when cancel() is calle dthe timer never runs again
//
void testCancel ()
{
    static const unsigned nTimers = 25u;
    cancelVerify *pTimers[nTimers];
    unsigned i;

    epicsTimerQueue &queue = epicsTimerQueue::allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new cancelVerify ( queue );
        assert ( pTimers[i] );
    }
    epicsTime cur = epicsTime::getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur + 4.0 );
    }
    epicsThreadSleep ( 5.0 );
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->cancel ();
    }
    epicsThreadSleep ( 1.0 );
    queue.release ();
}

class periodicVerify : public epicsTimerNotify {
public:
    periodicVerify ( epicsTimerQueue & );
    expireStatus expire ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void start ( const epicsTime &expireTime );
    void cancel ();
    void verifyCount ();
protected:
    virtual ~periodicVerify ();
private:
    unsigned nExpire;
    epicsTimer &timer;
    bool failOutIfExpireIsCalled;
    static tsFreeList < class periodicVerify, 0x20 > freeList;
};

periodicVerify::periodicVerify ( epicsTimerQueue &queue ) :
    nExpire ( 0u ), timer ( queue.createTimer ( *this ) ), failOutIfExpireIsCalled ( false )
        
{
}

periodicVerify::~periodicVerify ()
{
}

inline void periodicVerify::start ( const epicsTime &expireTime )
{
    this->timer.start ( expireTime );
}

tsFreeList < class periodicVerify, 0x20 > periodicVerify::freeList;

inline void * periodicVerify::operator new ( size_t size )
{
    return periodicVerify::freeList.allocate ( size );
}

inline void periodicVerify::operator delete ( void *pCadaver, size_t size )
{
    periodicVerify::freeList.release ( pCadaver, size );
}

inline void periodicVerify::cancel ()
{
    this->timer.cancel ();
    this->failOutIfExpireIsCalled = true;
}

inline void periodicVerify::verifyCount ()
{
    assert ( this->nExpire > 1u );
}

epicsTimerNotify::expireStatus periodicVerify::expire ()
{
    this->nExpire++;
    double root = 3.14159;
    for ( unsigned i = 0u; i < 10000; i++ ) {
        root = sqrt ( root );
    }
    assert ( ! this->failOutIfExpireIsCalled );
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

    epicsTimerQueue &queue = epicsTimerQueue::allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new periodicVerify ( queue );
        assert ( pTimers[i] );
    }
    epicsTime cur = epicsTime::getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur );
    }
    epicsThreadSleep ( 5.0 );
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->verifyCount ();
        pTimers[i]->cancel ();
    }
    epicsThreadSleep ( 1.0 );
    queue.release ();
}

void epicsTimerTest ()
{
    testAccuracy ();
    testCancel ();
    testPeriodic ();
    printf ( "test complete\n" );
}
