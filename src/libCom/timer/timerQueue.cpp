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
#include "epicsGuard.h"
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
    epicsGuard < epicsMutex > guard ( this->mutex );

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
    if ( this->timerList.first () ) {
        if ( currentTime >= this->timerList.first ()->exp ) {
            this->pExpireTmr = this->timerList.first ();
            this->timerList.remove ( *this->pExpireTmr ); 
            this->pExpireTmr->curState = timer::stateActive;
            this->processThread = epicsThreadGetIdSelf ();
#           ifdef DEBUG
                this->pExpireTmr->show ( 0u );
#           endif 
        }
        else {
            double delay = this->timerList.first ()->exp - currentTime;
            debugPrintf ( ( "no activity process %f to next\n", delay ) );
            return delay;
        }
    }
    else {
        return DBL_MAX;
    }

#   ifdef DEBUG
        unsigned N = 0u;
#   endif

    double delay = DBL_MAX;
    while ( true ) {
        epicsTimerNotify *pTmpNotify = this->pExpireTmr->pNotify;
        this->pExpireTmr->pNotify = 0;
        epicsTimerNotify::expireStatus expStat ( epicsTimerNotify::noRestart );

        {
            epicsGuardRelease < epicsMutex > unguard ( guard );

            debugPrintf ( ( "%5u expired \"%s\" with error %f sec\n", 
                N++, typeid ( this->pExpireTmr->notify ).name (), 
                currentTime - this->pExpireTmr->exp ) );

            expStat = pTmpNotify->expire ( currentTime );
        }

        //
        // only restart if they didnt cancel() the timer
        // while the call back was running
        //
        if ( this->cancelPending ) {
            this->pExpireTmr->curState = timer::stateLimbo;

            // 1) if another thread is canceling cancel() waits for this
            // 2) if this thread is canceling in the timer callback then
            // dont touch timer or notify here because the cancel might 
            // have occurred because they destroyed the timer in the 
            // callback
            this->cancelPending = false;
            this->cancelBlockingEvent.signal ();
        }
        else {
            // restart as nec
            if ( expStat.restart() ) {
                this->pExpireTmr->privateStart ( 
                    *pTmpNotify, currentTime + expStat.expirationDelay() );
            }
            else {
                this->pExpireTmr->curState = timer::stateLimbo;
            }
        }
        this->pExpireTmr = 0;

        if ( this->timerList.first () ) {
            if ( currentTime >= this->timerList.first ()->exp ) {
                this->pExpireTmr = this->timerList.first ();
                this->timerList.remove ( *this->pExpireTmr ); 
                this->pExpireTmr->curState = timer::stateActive;
#               ifdef DEBUG
                    this->pExpireTmr->show ( 0u );
#               endif 
            }
            else {
                delay = this->timerList.first ()->exp - currentTime;
                this->processThread = 0;
                break;
            }
        }
        else {
            this->processThread = 0;
            delay = DBL_MAX;
            break;
        }
    }
    return delay;
}

epicsTimer & timerQueue::createTimer ()
{
    return * new timer ( * this );
}

epicsTimerForC & timerQueue::createTimerForC ( epicsTimerCallback pCallback, void *pArg )
{
    return * new epicsTimerForC ( *this, pCallback, pArg );
}


void timerQueue::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    printf ( "epicsTimerQueue with %u items pending\n", this->timerList.count () );
    if ( level >= 1u ) {
        tsDLIterConstBD < timer > iter = this->timerList.firstIter ();
        while ( iter.valid () ) {   
            iter->show ( level - 1u );
            ++iter;
        }
    }
}
