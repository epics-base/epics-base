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

epicsTimerQueue::~epicsTimerQueue () {}

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

double timerQueue::process ( const epicsTime & currentTime )
{  
    {
        epicsAutoMutex locker ( this->mutex );

        if ( this->pExpireTmr ) {
            // if some other thread is processing the queue
            // (or if this is a recursive call)
            timer *pTmr = this->timerList.first ();
            if ( pTmr ) {
                double delay = pTmr->exp - currentTime;
                if ( delay < 0.0 ) {
                    delay = 0.0;
                }
                return delay;
            }
            else {
                return DBL_MAX;
            }
        }

        //
        // Tag current epired tmr so that we can detect if call back
        // is in progress when canceling the timer.
        //
        this->pExpireTmr = this->timerList.first ();
        if ( this->pExpireTmr ) {
            if ( currentTime >= this->pExpireTmr->exp ) {
                this->timerList.remove ( *this->pExpireTmr ); 
                this->pExpireTmr->curState = timer::stateLimbo;
                this->processThread = epicsThreadGetIdSelf ();
#               ifdef DEBUG
                    this->pExpireTmr->show ( 0u );
#               endif 
            }
            else {
                double delay = this->pExpireTmr->exp - currentTime;
                this->pExpireTmr = 0;
                debugPrintf ( ( "no activity process %f to next\n", delay ) );
                return delay;
            }
        }
        else {
            return DBL_MAX;
        }
    }

#   ifdef DEBUG
        unsigned N = 0u;
#   endif

    while ( true ) {
 
        debugPrintf ( ( "%5u expired \"%s\" with error %f sec\n", 
            N++, typeid ( this->pExpireTmr->notify ).name (), 
            currentTime - this->pExpireTmr->exp ) );

        epicsTimerNotify::expireStatus expStat = 
            this->pExpireTmr->pNotify->expire ( currentTime );

        epicsAutoMutex locker ( this->mutex );

        //
        // only restart if they didnt cancel() the timer
        // while the call back was running
        //
        if ( this->cancelPending ) {
            // cancel() waits for this
            this->cancelPending = false;
            this->cancelBlockingEvent.signal ();
            this->pExpireTmr->pNotify = 0;
        }
        // restart as nec
        else if ( expStat.restart() ) {
            this->pExpireTmr->privateStart ( 
                *this->pExpireTmr->pNotify, 
                currentTime + expStat.expirationDelay() );
        }
        else {
            this->pExpireTmr->pNotify = 0;
        }

        this->pExpireTmr = this->timerList.first ();
        if ( this->pExpireTmr ) {
            if ( currentTime >= this->pExpireTmr->exp ) {
                this->timerList.remove ( *this->pExpireTmr ); 
                this->pExpireTmr->curState = timer::stateLimbo;
#               ifdef DEBUG
                    this->pExpireTmr->show ( 0u );
#               endif 
            }
            else {
                double delay = this->pExpireTmr->exp - currentTime;
                this->pExpireTmr = 0;
                this->processThread = 0;
                return delay;
            }
        }
        else {
            this->processThread = 0;
            return DBL_MAX;
        }
    }
}

epicsTimer & timerQueue::createTimer ()
{
    epicsAutoMutex autoLock ( this->mutex );
    void *pBuf = this->timerFreeList.allocate ( sizeof (timer) );
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    return * new ( pBuf ) timer ( *this );
}

void timerQueue::destroyTimer ( epicsTimer & tmr ) 
{
    epicsAutoMutex autoLock ( this->mutex );
    tmr.~epicsTimer ();
    this->timerFreeList.release ( &tmr, sizeof( tmr ) );
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
