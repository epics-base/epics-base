/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <stdio.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "epicsGuard.h"
#include "timerPrivate.h"
#include "errlog.h"

const double timerQueue :: exceptMsgMinPeriod = 60.0 * 5.0; // seconds

epicsTimerQueue::~epicsTimerQueue () {}

timerQueue::timerQueue ( epicsTimerQueueNotify & notifyIn ) :
    notify ( notifyIn ), 
    pExpireTmr ( 0 ),  
    processThread ( 0 ), 
    exceptMsgTimeStamp ( 
        epicsTime :: getCurrent () - exceptMsgMinPeriod ),
    cancelPending ( false )
{
}

timerQueue::~timerQueue ()
{
    timer *pTmr;
    while ( ( pTmr = this->timerList.get () ) ) {    
        pTmr->curState = timer::stateLimbo;
    }
}

void timerQueue ::
    printExceptMsg ( const char * pName, const type_info & type )
{
    char date[64];
    double delay;
    try {
        epicsTime cur = epicsTime :: getCurrent ();
        delay = cur - this->exceptMsgTimeStamp;
        cur.strftime ( date, sizeof ( date ), 
                        "%a %b %d %Y %H:%M:%S.%f" );
        if ( delay >= exceptMsgMinPeriod ) {
            this->exceptMsgTimeStamp = cur;
        }
    }
    catch ( ... ) {
        delay = DBL_MAX;
        strcpy ( date, "UKN DATE" );
    }
    if ( delay >= exceptMsgMinPeriod ) {
        // we dont touch the typeid for the timer expiration
        // notify interface here because they might have 
        // destroyed the timer during its callback
        errlogPrintf ( 
            "timerQueue: Unexpected C++ exception \"%s\" "
            "with type \"%s\" during timer expiration "
            "callback at %s\n",
            pName, 
            type.name (), 
            date );
        errlogFlush ();
    }
}

double timerQueue::process ( const epicsTime & currentTime )
{  
    epicsGuard < epicsMutex > guard ( this->mutex );

    if ( this->pExpireTmr ) {
        // if some other thread is processing the queue
        // (or if this is a recursive call)
        timer * pTmr = this->timerList.first ();
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
            try {
                expStat = pTmpNotify->expire ( currentTime );
            }
            catch ( std::exception & except ) {
                printExceptMsg ( except.what (), typeid ( except ) );
            }
            catch ( ... ) {
                printExceptMsg ( "non-standard exception", typeid ( void ) );
            }
        }

        //
        // only restart if they didnt cancel() the timer
        // while the call back was running
        //
        if ( this->cancelPending ) {
            // 1) if another thread is canceling then cancel() waits for 
            // the event below
            // 2) if this thread is canceling in the timer callback then
            // dont touch timer or notify here because the cancel might 
            // have occurred because they destroyed the timer in the 
            // callback
            this->cancelPending = false;
            this->cancelBlockingEvent.signal ();
        }
        else {
            this->pExpireTmr->curState = timer::stateLimbo;
            if ( this->pExpireTmr->pNotify ) {
                // pNotify was cleared above so if it is valid now we know that
                // someone has started the timer from another thread and that 
                // predominates over the restart parameters from expire.
                this->pExpireTmr->privateStart ( 
                    *this->pExpireTmr->pNotify, this->pExpireTmr->exp );
            }
            else if ( expStat.restart() ) {
                // restart as nec
                this->pExpireTmr->privateStart ( 
                    *pTmpNotify, currentTime + expStat.expirationDelay() );
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
    return * new ( this->timerFreeList ) timer ( * this );
}

epicsTimerForC & timerQueue::createTimerForC ( epicsTimerCallback pCallback, void *pArg )
{
    return * new ( this->timerForCFreeList ) epicsTimerForC ( *this, pCallback, pArg );
}

void timerQueue::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    printf ( "epicsTimerQueue with %u items pending\n", this->timerList.count () );
    if ( level >= 1u ) {
        tsDLIterConst < timer > iter = this->timerList.firstIter ();
        while ( iter.valid () ) {   
            iter->show ( level - 1u );
            ++iter;
        }
    }
}
