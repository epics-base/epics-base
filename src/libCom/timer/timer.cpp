
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

tsFreeList < class timer, 0x20 > timer::freeList;

epicsTimer::~epicsTimer () {}

timer::timer ( epicsTimerNotify &notifyIn, timerQueue &queueIn ) :
    curState ( stateLimbo ), notify ( notifyIn ), queue ( queueIn )
{
}

timer::~timer()
{
    this->cancel ();
}

void timer::start ( double delaySeconds )
{
    this->start ( epicsTime::getCurrent () + delaySeconds );
}

void timer::start ( const epicsTime & expire )
{
    epicsAutoMutex locker ( this->queue.mutex );
    this->privateStart ( expire );
}

void timer::privateStart ( const epicsTime & expire )
{
    this->privateCancel ();

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
    
#   ifdef DEBUG 
        this->show ( 10u );
        this->queue.show ( 10u );
#   endif

    debugPrintf ( ("Arm of \"%s\" with delay %f at %p preempting %u\n", 
        typeid ( this->notify ).name (), 
        expire - epicsTime::getCurrent (), 
        this, preemptCount ) );
}

void timer::cancel ()
{
    {
        epicsAutoMutex locker ( this->queue.mutex );
        this->privateCancel ();
        // dont wait if this was called indirectly by expire ()
        if ( this->queue.pExpireTmr == this && 
            this->queue.processThread != epicsThreadGetIdSelf () ) {
            this->queue.cancelPending = true;
        }
    }

    // make certain timer expire() does not run after cancel () returns,
    // but dont require that lock is applied while calling expire()
    if ( this->queue.cancelPending ) {
        while ( this->queue.cancelPending ) {
            this->queue.cancelBlockingEvent.wait ();
        }
        // in case other threads are waiting
        this->queue.cancelBlockingEvent.signal ();
    }
}

void timer::privateCancel ()
{
    if ( this->curState == statePending ) {
        this->queue.timerList.remove ( *this );
        this->curState = stateLimbo;
    }
}

double timer::getExpireDelay () const
{
    epicsAutoMutex locker ( this->queue.mutex );
    return this->privateDelayToFirstExpire ();
}

void timer::show ( unsigned int level ) const
{
    double delay = this->getExpireDelay ();
    printf ( "%s, state = %s, delay to expire = %g\n", 
        typeid ( this->notify ).name (), 
        this->curState == statePending ? "pending" : "limbo",
        delay );
    if ( level >= 1u ) {
        this->notify.show ( level - 1u );
    }
}
