
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

#define epicsExportSharedSymbols
#include "timerPrivate.h"

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
    epicsAutoMutex autoLock ( this->queue.mutex );
    this->~timer ();
    this->queue.timerFreeList.release ( this, sizeof( *this ) );
}

void timer::start ( epicsTimerNotify & notify, double delaySeconds )
{
    this->start ( notify, epicsTime::getCurrent () + delaySeconds );
}

void timer::start ( epicsTimerNotify & notify, const epicsTime & expire )
{
    epicsAutoMutex locker ( this->queue.mutex );
    this->privateCancel ();
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
    epicsAutoMutex locker ( this->queue.mutex );
    this->privateCancel ();
}

void timer::privateCancel ()
{
    while ( true ) {
        if ( this->curState == statePending ) {
	    if ( this->queue.timerList.first() == this ) {
            	this->queue.notify.reschedule ();
            }
            this->queue.timerList.remove ( *this );
            this->curState = stateLimbo;
            this->pNotify = 0;
        }
        if ( this->queue.pExpireTmr == this && 
            this->queue.processThread != epicsThreadGetIdSelf() ) {
            this->queue.cancelPending = true;
            // make certain timer expire() does not run after cancel () returns,
            // but dont require that lock is applied while calling expire()
            {
                epicsAutoMutexRelease autoRelease ( this->queue.mutex );
                while ( this->queue.cancelPending ) {
                    this->queue.cancelBlockingEvent.wait ();
                }
                // in case other threads are waiting
                this->queue.cancelBlockingEvent.signal ();
            }
        }
        else {
            // dont wait if this was called indirectly by expire ()
            return;
        }
    }
}

epicsTimer::expireInfo timer::getExpireInfo () const
{
    epicsAutoMutex locker ( this->queue.mutex );
    if ( this->curState == statePending || this->queue.pExpireTmr == this ) {
        return expireInfo ( true, this->exp );
    }
    else {
        return expireInfo ( false, epicsTime() );
    }
}

void timer::show ( unsigned int level ) const
{
    epicsAutoMutex locker ( this->queue.mutex );
    const char * pName = "<no notify attached>";
    if ( this->pNotify ) {
        pName = typeid ( *this->pNotify ).name ();
    }
    double delay;
    if ( this->curState == statePending ) {
        delay = this->exp - epicsTime::getCurrent();
    }
    else {
        delay = -DBL_MAX;
    }
    printf ( "%s, state = %s, delay = %f\n",
        pName, this->curState == statePending ? "pending" : "limbo",
        delay );
    if ( level >= 1u && this->pNotify ) {
        this->pNotify->show ( level - 1u );
    }
}

timerQueue & timer::getPrivTimerQueue()
{
    return this->queue;
}

