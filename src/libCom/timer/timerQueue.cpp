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
#include "epicsTimerPrivate.h"

timerQueue::timerQueue ( epicsTimerQueueNotify &notifyIn ) :
    notify ( notifyIn ), pExpireTmr ( 0 ),  
        processThread ( 0 ), cancelPending ( false )
{
}

timerQueue::~timerQueue ()
{
    timer *pTmr;
    this->mutex.lock ();
    while ( ( pTmr = this->timerList.get () ) ) {    
        pTmr->curState = timer::stateLimbo;
    }
}

double timerQueue::delayToFirstExpire () const
{
    epicsAutoMutex locker ( this->mutex );
    timer *pTmr = this->timerList.first ();
    if ( pTmr ) {
        return pTmr->privateDelayToFirstExpire ();
    }
    else {
        return DBL_MAX;
    }
}

void timerQueue::process ()
{
    epicsTime cur ( epicsTime::getCurrent () );
  
    {
        epicsAutoMutex locker ( this->mutex );

        if ( this->pExpireTmr ) {
            // if some other thread is processing the queue
            // (or if this is a recursive call)
            return; 
        }

        this->pExpireTmr = this->timerList.get ();
        if ( ! this->pExpireTmr ) {
            return;
        }
        if ( cur > this->pExpireTmr->exp ) {

            //
            // Tag current epired tmr so that we can detect if call back
            // is in progress when canceling the timer.
            //
#           ifdef DEBUG
                this->pExpireTmr->show ( 0u );
#           endif 
            this->pExpireTmr->curState = timer::stateLimbo;
            this->processThread = epicsThreadGetIdSelf ();
        }
        else {
            // no activity
            debugPrintf ( ( "no activity process\n" ) );
            this->timerList.push ( *this->pExpireTmr );
            this->pExpireTmr = 0;
            return;
        }
    }

#   ifdef DEBUG
        unsigned N = 0u;
#   endif

    while ( true ) {
 
        debugPrintf ( ( "%5u expired \"%s\" with error %f sec\n", 
            N++, typeid ( this->pExpireTmr->notify ).name (), 
            cur - this->pExpireTmr->exp ) );

        epicsTimerNotify::expireStatus expStat = this->pExpireTmr->notify.expire ();

        epicsAutoMutex locker ( this->mutex );

        //
        // only restart if they didnt cancel() the timer
        // while the call back was running
        //
        if ( this->cancelPending ) {
            // cancel () waits for this
            this->cancelPending = false;
            this->cancelBlockingEvent.signal ();
        }
        // restart as nec
        else if ( expStat.restart () ) {
            this->pExpireTmr->privateStart ( cur + expStat.expirationDelay () );
        }

        this->pExpireTmr = this->timerList.get ();
        if ( ! this->pExpireTmr ) {
            this->processThread = 0;
            return;
        }
        if ( cur > this->pExpireTmr->exp ) {
#           ifdef DEBUG
                this->pExpireTmr->show ( 0u );
#           endif 
            this->pExpireTmr->curState = timer::stateLimbo;
        }
        else {
            // no activity
            this->timerList.push ( *this->pExpireTmr );
            this->pExpireTmr = 0;
            this->processThread = 0;
            return;
        }
    }
}

void timerQueue::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    printf ( "epicsTimerQueue with %u items pending\n", this->timerList.count () );
    if ( level >= 1u ) {
        tsDLIterConstBD < timer > iter = this->timerList.firstIter ();
        while ( iter.valid () ) {   
            iter->show ( level - 1u );
            ++iter;
        }
    }
}
