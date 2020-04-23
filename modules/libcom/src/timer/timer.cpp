/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <typeinfo>
#include <string>
#include <stdexcept>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsGuard.h"
#include "timerPrivate.h"
#include "errlog.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class tsFreeList < timer, 0x20 >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

timer::timer ( timerQueue & queueIn ) :
    queue ( queueIn ), curState ( stateLimbo ), pNotify ( 0 ) 
{
}

timer::~timer ()
{
    this->cancel ();
}

void timer::destroy () 
{
    timerQueue & queueTmp = this->queue;
    this->~timer ();
    queueTmp.timerFreeList.release ( this );
}

void timer::start ( epicsTimerNotify & notify, double delaySeconds )
{
    this->start ( notify, epicsTime::getCurrent () + delaySeconds );
}

void timer::start ( epicsTimerNotify & notify, const epicsTime & expire )
{
    epicsGuard < epicsMutex > locker ( this->queue.mutex );
    this->privateStart ( notify, expire );
}

void timer::privateStart ( epicsTimerNotify & notify, const epicsTime & expire )
{
    this->pNotify = & notify;
    this->exp = expire - ( this->queue.notify.quantum () / 2.0 );

    bool reschedualNeeded = false;
    if ( this->curState == stateActive ) {
        // above expire time and notify will override any restart parameters
        // that may be returned from the timer expire callback
        return;
    }
    else if ( this->curState == statePending ) {
        this->queue.timerList.remove ( *this );
        if ( this->queue.timerList.first() == this && 
                this->queue.timerList.count() > 0 ) {
            reschedualNeeded = true;
        }
    }

#   ifdef DEBUG
        unsigned preemptCount=0u;
#   endif

    //
    // insert into the pending queue
    //
    // Finds proper time sorted location using a linear search.
    //
    // **** this should use a binary tree ????
    //
    tsDLIter < timer > pTmr = this->queue.timerList.lastIter ();
    while ( true ) {
        if ( ! pTmr.valid () ) {
            //
            // add to the beginning of the list
            //
            this->queue.timerList.push ( *this );
            reschedualNeeded = true;
            break;
        }
        if ( pTmr->exp <= this->exp ) {
            //
            // add after the item found that expires earlier
            //
            this->queue.timerList.insertAfter ( *this, *pTmr );
            break;
        }
#       ifdef DEBUG
            preemptCount++;
#       endif
        --pTmr;
    }

    this->curState = timer::statePending;

    if ( reschedualNeeded ) {
        this->queue.notify.reschedule ();
    }
    
#   if defined(DEBUG) && 0
        this->show ( 10u );
        this->queue.show ( 10u );
#   endif

    debugPrintf ( ("Start of \"%s\" with delay %f at %p preempting %u\n", 
        typeid ( this->notify ).name (), 
        expire - epicsTime::getCurrent (), 
        this, preemptCount ) );
}

void timer::cancel ()
{
    bool reschedual = false;
    bool wakeupCancelBlockingThreads = false;
    {
        epicsGuard < epicsMutex > locker ( this->queue.mutex );
        this->pNotify = 0;
        if ( this->curState == statePending ) {
            this->queue.timerList.remove ( *this );
            this->curState = stateLimbo;
            if ( this->queue.timerList.first() == this && 
                    this->queue.timerList.count() > 0 ) {
                reschedual = true;
            }
        }
        else if ( this->curState == stateActive ) {
            this->queue.cancelPending = true;
            this->curState = timer::stateLimbo;
            if ( this->queue.processThread != epicsThreadGetIdSelf() ) {
                // make certain timer expire() does not run after cancel () returns,
                // but dont require that lock is applied while calling expire()
                while ( this->queue.cancelPending && 
                        this->queue.pExpireTmr == this ) {
                    epicsGuardRelease < epicsMutex > autoRelease ( locker );
                    this->queue.cancelBlockingEvent.wait ();
                }
                // in case other threads are waiting
                wakeupCancelBlockingThreads = true;
            }
        }
    }
    if ( reschedual ) {
        this->queue.notify.reschedule ();
    }
    if ( wakeupCancelBlockingThreads ) {
        this->queue.cancelBlockingEvent.signal ();
    }
}

epicsTimer::expireInfo timer::getExpireInfo () const
{
    // taking a lock here guarantees that users will not 
    // see brief intervals when a timer isnt active because
    // it is is canceled when start is called
    epicsGuard < epicsMutex > locker ( this->queue.mutex );
    if ( this->curState == statePending || this->curState == stateActive ) {
        return expireInfo ( true, this->exp );
    }
    return expireInfo ( false, epicsTime() );
}

void timer::show ( unsigned int level ) const
{
    epicsGuard < epicsMutex > locker ( this->queue.mutex );
    double delay;
    if ( this->curState == statePending || this->curState == stateActive ) {
        try {
            delay = this->exp - epicsTime::getCurrent();
        }
        catch ( ... ) {
            delay = - DBL_MAX;
        }
    }
    else {
        delay = -DBL_MAX;
    }
    const char *pStateName;
    if ( this->curState == statePending ) {
        pStateName = "pending";
    }
    else if ( this->curState == stateActive ) {
        pStateName = "active";
    }
    else if ( this->curState == stateLimbo ) {
        pStateName = "limbo";
    }
    else {
        pStateName = "corrupt";
    }
    printf ( "timer, state = %s, delay = %f\n",
        pStateName, delay );
    if ( level >= 1u && this->pNotify ) {
        this->pNotify->show ( level - 1u );
    }
}

void timer::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void epicsTimerForC::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

