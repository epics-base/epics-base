
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

#include <typeinfo>

#define epicsExportSharedSymbols
#include "epicsGuard.h"
#include "timerPrivate.h"

template class tsFreeList < timer, 0x20 >;
template class epicsSingleton < tsFreeList < timer, 0x20 > >;

epicsSingleton < tsFreeList < timer, 0x20 > > timer::pFreeList;

timer::timer ( timerQueue &queueIn ) :
    curState ( stateLimbo ), pNotify ( 0 ), queue ( queueIn )
{
}

timer::~timer ()
{
    this->cancel ();
}

void timer::destroy () 
{
    delete this;
}

void timer::start ( epicsTimerNotify & notify, double delaySeconds )
{
    this->start ( notify, epicsTime::getCurrent () + delaySeconds );
}

void timer::start ( epicsTimerNotify & notify, const epicsTime & expire )
{
    epicsGuard < epicsMutex > locker ( this->queue.mutex );
    this->privateCancel ( locker );
    this->privateStart ( notify, expire );
}

void timer::privateStart ( epicsTimerNotify & notify, const epicsTime & expire )
{
    this->pNotify = & notify;
    this->exp = expire;

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
    tsDLIterBD < timer > pTmr = this->queue.timerList.lastIter ();
    while ( true ) {
        if ( ! pTmr.valid () ) {
            //
            // add to the beginning of the list
            //
            this->queue.timerList.push ( *this );
            this->queue.notify.reschedule ();
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
    if ( this->curState == statePending || this->curState == stateActive ) {
        epicsGuard < epicsMutex > locker ( this->queue.mutex );
        this->privateCancel ( locker );
    }
}

void timer::privateCancel ( epicsGuard < epicsMutex > & locker )
{
    if ( this->curState == statePending ) {
        if ( this->queue.timerList.first() == this ) {
            this->queue.notify.reschedule ();
        }
        this->queue.timerList.remove ( *this );
        this->curState = stateLimbo;
        this->pNotify = 0;
    }
    else if ( this->curState == stateActive ) {
        this->queue.cancelPending = true;
        this->curState = timer::stateLimbo;
        if ( this->queue.processThread != epicsThreadGetIdSelf() ) {
            // make certain timer expire() does not run after cancel () returns,
            // but dont require that lock is applied while calling expire()
            {
                epicsGuardRelease < epicsMutex > autoRelease ( locker );
                while ( this->queue.cancelPending && 
                        this->queue.pExpireTmr == this ) {
                    this->queue.cancelBlockingEvent.wait ();
                }
                // in case other threads are waiting
                this->queue.cancelBlockingEvent.signal ();
            }
        }
    }
}

epicsTimer::expireInfo timer::getExpireInfo () const
{
    if ( this->curState == statePending || this->curState == stateActive ) {
        return expireInfo ( true, this->exp );
    }
    return expireInfo ( false, epicsTime() );
}

void timer::show ( unsigned int level ) const
{
    epicsGuard < epicsMutex > locker ( this->queue.mutex );
    const char * pName = "<no notify attached>";
    const char *pStateName;

    if ( this->pNotify ) {
        pName = typeid ( *this->pNotify ).name ();
    }
    double delay;
    if ( this->curState == statePending || this->curState == stateActive ) {
        delay = this->exp - epicsTime::getCurrent();
    }
    else {
        delay = -DBL_MAX;
    }
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
    printf ( "%s, state = %s, delay = %f\n",
        pName, pStateName, delay );
    if ( level >= 1u && this->pNotify ) {
        this->pNotify->show ( level - 1u );
    }
}

timerQueue & timer::getPrivTimerQueue()
{
    return this->queue;
}

