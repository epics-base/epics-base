
/* * $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <limits.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "tsMinMax.h"

#include "iocinf.h"
#include "searchTimer.h"

#define epicsExportSharedSymbols
#include "udpiiu.h"
#undef epicsExportSharedSymbols

static const unsigned maxSearchTries = 100u; //  max tries on unchanged net 
static const unsigned initialTriesPerFrame = 1u; // initial UDP frames per search try
static const unsigned maxTriesPerFrame = 64u; // max UDP frames per search try 

static const double initialRoundTripEstimate = 0.250; // seconds
static const double minSearchPeriod = 0.030; // seconds
static const double maxSearchPeriod = 5.0; // seconds

//
// searchTimer::searchTimer ()
//
searchTimer::searchTimer ( udpiiu &iiuIn, epicsTimerQueue &queueIn, epicsMutex &mutexIn ) :
    period ( initialRoundTripEstimate * 2.0 ),
    roundTripDelayEstimate ( initialRoundTripEstimate ),
    timer ( queueIn.createTimer () ),
    mutex ( mutexIn ),
    iiu ( iiuIn ),
    framesPerTry ( initialTriesPerFrame ),
    framesPerTryCongestThresh ( UINT_MAX ),
    minRetry ( UINT_MAX ),
    retry ( 0u ),
    searchAttempts ( 0u ),
    searchResponses ( 0u ),
    searchAttemptsThisPass ( 0u ), 
    searchResponsesThisPass ( 0u ), 
    retrySeqNo ( 0u ),
    retrySeqAtPassBegin ( 0u ),
    active ( false ),
    noDelay ( false )
{
}

searchTimer::~searchTimer ()
{
    this->timer.destroy ();
}

//
// searchTimer::resetPeriod ()
//
void searchTimer::resetPeriod ( double delayToNextTry )
{
    bool start;

    // upper bound
    double newPeriod = this->roundTripDelayEstimate * 2.0;
    if ( newPeriod > initialRoundTripEstimate * 2.0 ) {
        newPeriod = initialRoundTripEstimate * 2.0;
    }
    // lower bound
    if ( newPeriod < minSearchPeriod ) {
        newPeriod = minSearchPeriod;
    }
        
    this->retry = 0;
    if ( this->iiu.channelCount() > 0 ) {
        if ( ! this->active ) {
            this->active = true;
            this->noDelay = ( delayToNextTry == 0.0 );
            start = true;
        }
        else if ( this->period > newPeriod ) {
            double delay = this->timer.getExpireDelay();
            if ( delay > newPeriod ) {
                this->active = true;
                this->noDelay = ( delayToNextTry == 0.0 );
                delayToNextTry = newPeriod;
                start = true;
            }
            else {
                start = false;
            }
        }
        else {
            start = false;
        }
    }
    else {
        start = false;
    }

    this->period = newPeriod;

    if ( start ) {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        this->timer.start ( *this, delayToNextTry );
        // debugPrintf ( ("rescheduled search timer for completion in %f sec\n", delayToNextTry) );
    }
}

/* 
 * searchTimer::setRetryInterval ()
 */
void searchTimer::setRetryInterval ( unsigned retryNo )
{
    unsigned idelay;
    double delay;

    /*
     * set the retry number
     */
    this->retry = tsMin ( retryNo, maxSearchTries + 1u );

    /*
     * set the retry interval
     */
    idelay = 1u << tsMin ( static_cast < size_t > ( this->retry ), 
                                CHAR_BIT * sizeof ( idelay ) - 1u );
    delay = idelay * this->roundTripDelayEstimate * 2.0; /* sec */ 
    //delay = idelay * initialRoundTripEstimate * 2.0; /* sec */ 

    this->period = tsMin ( maxSearchPeriod, delay );
    this->period = tsMax ( minSearchPeriod, this->period );

    debugPrintf ( ("new CA search period is %f sec\n", this->period) );
}

//
// searchTimer::notifySearchResponse ()
//
// Reset the delay to the next search request if we get
// at least one response. However, dont reset this delay if we
// get a delayed response to an old search request.
//
void searchTimer::notifySearchResponse ( unsigned short retrySeqNoIn, 
                                        const epicsTime & currentTime )
{
    bool reschedualNeeded;

    if ( this->retrySeqAtPassBegin <= retrySeqNoIn ) {
        if ( this->searchResponses < UINT_MAX ) {
            this->searchResponses++;
        }
    }    

    if ( retrySeqNoIn == this->retrySeqNo && ! this->noDelay ) {
        double curRTT = currentTime - this->timeAtLastRetry;
        this->roundTripDelayEstimate = 
            ( this->roundTripDelayEstimate + curRTT ) / 2.0;
        this->period = this->roundTripDelayEstimate * 2.0;
        this->period = tsMin ( maxSearchPeriod, this->period );
        this->period = tsMax ( minSearchPeriod, this->period );
        reschedualNeeded = true;
        this->active = true;
        this->noDelay = true;
    }

    if ( this->searchResponses == this->searchAttempts ) {
        reschedualNeeded = true;
        this->active = true;
        this->noDelay = true;
    }
    else {
        reschedualNeeded = false;
    }

    if ( reschedualNeeded ) {
        epicsAutoMutexRelease autoRelease (this->mutex );
#       if defined(DEBUG) && 0
            char buf[64];
            epicsTime ts = epicsTime::getCurrent();
            ts.strftime ( buf, sizeof(buf), "%M:%S.%09f");
#       endif
        // debugPrintf ( ( "Response set timer delay to zero. ts=%s, RTT=%f sec\n", 
        //    buf, this->roundTripDelayEstimate ) );
        this->timer.start ( *this, currentTime );
    }
}

//
// searchTimer::expire ()
//
epicsTimerNotify::expireStatus searchTimer::expire ( const epicsTime & currentTime )
{
    epicsAutoMutex locker ( this->mutex );
    unsigned nFrameSent = 0u;
    unsigned nChanSent = 0u;

    /*
     * check to see if there is nothing to do here 
     */
    if ( this->iiu.channelCount () == 0 ) {
        this->active = false;
        this->noDelay = false;
        debugPrintf ( ( "all channels located - search timer terminating\n" ) );
        return noRestart;
    }   

    if ( ! this->noDelay ) {
        debugPrintf ( ( "timed out waiting for a response\n" ) );
    }

    /*
     * increment the retry sequence number
     */
    this->retrySeqNo++; /* allowed to roll over */
    this->timeAtLastRetry = currentTime;

    /*
     * dynamically adjust the number of UDP frames per 
     * try depending how many search requests are not 
     * replied to
     *
     * This determines how many search request can be 
     * sent together (at the same instant in time).
     *
     * The variable this->framesPerTry
     * determines the number of UDP frames to be sent
     * each time that expire() is called.
     * If this value is too high we will waste some
     * network bandwidth. If it is too low we will
     * use very little of the incoming UDP message
     * buffer associated with the server's port and
     * will therefore take longer to connect. We 
     * initialize this->framesPerTry
     * to a prime number so that it is less likely that the
     * same channel is in the last UDP frame
     * sent every time that this is called (and
     * potentially discarded by a CA server with
     * a small UDP input queue). 
     */
    /*
     * increase frames per try only if we see better than
     * a 93.75% success rate for one pass through the list
     */
    if ( this->searchResponses >
        ( this->searchAttempts - (this->searchAttempts/16u) ) ) {
        /*
         * increase UDP frames per try if we have a good score
         */
        if ( this->framesPerTry < maxTriesPerFrame ) {
            /*
             * a congestion avoidance threshold similar to TCP is now used
             */
            if ( this->framesPerTry < this->framesPerTryCongestThresh ) {
                this->framesPerTry += this->framesPerTry;
            }
            else {
                this->framesPerTry += (this->framesPerTry/8) + 1;
            }
            debugPrintf ( ("Increasing frame count to %u t=%u r=%u\n", 
                this->framesPerTry, this->searchAttempts, this->searchResponses) );
        }
    }
    /*
     * if we detect congestion because we have less than a 87.5% success 
     * rate then gradually reduce the frames per try
     */
    else if ( this->searchResponses < 
        ( this->searchAttempts - (this->searchAttempts/8u) ) ) {
        if (this->framesPerTry>1) {
            this->framesPerTry--;
        }
        this->framesPerTryCongestThresh = this->framesPerTry/2 + 1;
        debugPrintf ( ("Congestion detected - set frames per try to %u t=%u r=%u\n", 
            this->framesPerTry, this->searchAttempts, this->searchResponses) );
    }

    if ( this->searchAttemptsThisPass <= UINT_MAX - this->searchAttempts ) {
        this->searchAttemptsThisPass += this->searchAttempts;
    }
    else {
        this->searchAttemptsThisPass = UINT_MAX;
    }
    if ( this->searchResponsesThisPass <= UINT_MAX - this->searchResponses ) {
        this->searchResponsesThisPass += this->searchResponses;
    }
    else {
        this->searchResponsesThisPass = UINT_MAX;
    }
    this->searchAttempts = 0;
    this->searchResponses = 0;

    while ( 1 ) {
            
        /*
         * clear counter when we reach the end of the list
         *
         * if we are making some progress then
         * dont increase the delay between search
         * requests
         */
        if ( this->searchAttemptsThisPass >= this->iiu.channelCount () ) {
            if ( this->searchResponsesThisPass == 0u ) {
                debugPrintf ( ("increasing search try interval\n") );
                this->setRetryInterval ( this->minRetry + 1u );
            }
        
            this->minRetry = UINT_MAX;
        
            /*
             * increment the retry sequence number
             * (this prevents the time of the next search
             * try from being set to the current time if
             * we are handling a response from an old
             * search message)
             */
            this->retrySeqNo++; /* allowed to roll over */
        
            /*
             * so that old search tries will not update the counters
             */
            this->retrySeqAtPassBegin = this->retrySeqNo;

            this->searchAttemptsThisPass = 0;
            this->searchResponsesThisPass = 0;

            debugPrintf ( ("saw end of list\n") );
        }
    
        unsigned retryNoForThisChannel;
        if ( ! this->iiu.searchMsg ( this->retrySeqNo, retryNoForThisChannel ) ) {
            nFrameSent++;
        
            if ( nFrameSent >= this->framesPerTry ) {
                break;
            }

            this->iiu.datagramFlush ();
       
            if ( ! this->iiu.searchMsg ( this->retrySeqNo, retryNoForThisChannel ) ) {
                break;
            }
        }

        if ( this->minRetry > retryNoForThisChannel ) {
            this->minRetry = retryNoForThisChannel;
        }

        if ( this->searchAttempts < UINT_MAX ) {
            this->searchAttempts++;
        }
        if ( nChanSent < UINT_MAX ) {
            nChanSent++;
        }

        /*
         * dont send any of the channels twice within one try
         */
        if ( nChanSent >= this->iiu.channelCount () ) {
            /*
             * add one to nFrameSent because there may be 
             * one more partial frame to be sent
             */
            nFrameSent++;
        
            /* 
             * cap this->framesPerTry to
             * the number of frames required for all of 
             * the unresolved channels
             */
            if ( this->framesPerTry > nFrameSent ) {
                this->framesPerTry = nFrameSent;
            }
        
            break;
        }
    }

    // flush out the search request buffer
    this->iiu.datagramFlush ();

#   ifdef DEBUG
        char buf[64];
        epicsTime ts = epicsTime::getCurrent();
        ts.strftime ( buf, sizeof(buf), "%M:%S.%09f");
        debugPrintf ( ("sent %u delay sec=%f RTT=%f ts=%s\n", 
            nFrameSent, this->period, 
            this->roundTripDelayEstimate, buf ) );
#   endif

    if ( this->iiu.channelCount () == 0 ) {
        debugPrintf ( ( "all channels connected\n" ) );
        this->active = false;
        this->noDelay = false;
        return noRestart;
    }
    else if ( this->retry < maxSearchTries ) {
        this->noDelay = this->period == 0.0;
        return expireStatus ( restart, this->period );
    }
    else {
        debugPrintf ( ( "maximum search tries exceeded - giving up\n" ) );
        this->active = false;
        this->noDelay = false;
        return noRestart;
    }
}

void searchTimer::show ( unsigned /* level */ ) const
{
}

