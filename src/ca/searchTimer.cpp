/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// 
// $Id$
//
//                    L O S  A L A M O S
//              Los Alamos National Laboratory
//               Los Alamos, New Mexico 87545
//
//  Copyright, 1986, The Regents of the University of California.
//
//  Author: Jeff Hill
//

#include <limits.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "tsMinMax.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "searchTimer.h"
#include "udpiiu.h"

static const unsigned maxSearchTries = 100u; //  max tries on unchanged net 
static const unsigned initialTriesPerFrame = 1u; // initial UDP frames per search try
static const unsigned maxTriesPerFrame = 64u; // max UDP frames per search try 

static const double minRoundTripEstimate = 100e-6; // seconds
static const double maxRoundTripEstimate = 5.0; // seconds
static const double minSearchPeriod = 30e-3; // seconds
static const double maxSearchPeriod = 5.0; // seconds

//
// searchTimer::searchTimer ()
//
searchTimer::searchTimer ( udpiiu & iiuIn, epicsTimerQueue & queueIn ) :
    period ( 1e9 ),
    timer ( queueIn.createTimer () ),
    iiu ( iiuIn ),
    framesPerTry ( initialTriesPerFrame ),
    framesPerTryCongestThresh ( UINT_MAX ),
    minRetry ( 0 ),
    minRetryThisPass ( UINT_MAX ),
    searchAttempts ( 0u ),
    searchResponses ( 0u ),
    searchAttemptsThisPass ( 0u ),
    searchResponsesThisPass ( 0u ), 
    dgSeqNoAtTimerExpireBegin ( 0u ),
    dgSeqNoAtTimerExpireEnd ( 0u )
{
}

searchTimer::~searchTimer ()
{
    this->timer.destroy ();
}

void searchTimer::newChannleNotify ( bool firstChannel )
{
    if ( firstChannel ) {
        {
            epicsGuard < searchTimerMutex > locker ( this->mutex );
            this->recomputeTimerPeriod ( 0 );
        }
        this->timer.start ( *this, this->period );
    }
    else {
        this->recomputeTimerPeriodAndStartTimer ( 0, 0.0 );
    }
}

void searchTimer::beaconAnomalyNotify ( const double & delay )
{
    this->recomputeTimerPeriodAndStartTimer ( beaconAnomalyRetrySetpoint, delay );
}

// lock must be applied
void searchTimer::recomputeTimerPeriod ( unsigned minRetryNew )
{
    this->minRetry = minRetryNew;

    size_t retry = static_cast < size_t > 
        ( tsMin ( this->minRetry, maxSearchTries + 1u ) );

    unsigned idelay = 1u << tsMin ( retry, CHAR_BIT * sizeof ( idelay ) - 1u );
    this->period = idelay * this->iiu.roundTripDelayEstimate() * 2.0; /* sec */ 
    this->period = tsMin ( maxSearchPeriod, this->period );
    this->period = tsMax ( minSearchPeriod, this->period );
}

void searchTimer::recomputeTimerPeriodAndStartTimer ( unsigned minRetryNew, const double & initialDelay )
{
    bool start = false;
    double totalDelay = initialDelay;

    epicsGuard < searchTimerMutex > locker ( this->mutex );

    if ( this->iiu.channelCount() == 0 || this->minRetry <= minRetryNew  ) {
        return; 
    }

    double oldPeriod = this->period;

    this->recomputeTimerPeriod ( minRetryNew );

    totalDelay += this->period;

    if ( totalDelay < oldPeriod ) {
        epicsTimer::expireInfo info = this->timer.getExpireInfo ();
        if ( info.active ) {
            double delay = epicsTime::getCurrent() - info.expireTime;
            if ( delay > totalDelay ) {
                start = true;
            }
        }
        else {
            start = true;
        }
    }

    if ( start ) {
        this->timer.start ( *this, totalDelay );
    }
    debugPrintf ( ( "changed search period to %f sec\n", this->period ) );
}

//
// searchTimer::notifySearchResponse ()
//
// Reset the delay to the next search request if we get
// at least one response. However, dont reset this delay if we
// get a delayed response to an old search request.
//
void searchTimer::notifySearchResponse ( ca_uint32_t respDatagramSeqNo, 
                     bool seqNumberIsValid, const epicsTime & currentTime )
{
    bool reschedualNeeded = false;

    if ( this->iiu.channelCount() > 0 ) {
        epicsGuard < searchTimerMutex > locker ( this->mutex );

        if ( seqNumberIsValid ) {
            if ( this->dgSeqNoAtTimerExpireBegin <= respDatagramSeqNo && 
                    this->dgSeqNoAtTimerExpireEnd >= respDatagramSeqNo ) {
                if ( this->searchResponses < UINT_MAX ) {
                    this->searchResponses++;
                }
            }    
        }
        else if ( this->searchResponses < UINT_MAX ) {
            this->searchResponses++;
        }

        //
        // when we get 100% success immediately send another search request
        //
        if ( this->searchResponses == this->searchAttempts ) {
            reschedualNeeded = true;
        }
    }

    if ( reschedualNeeded ) {
#       if defined(DEBUG) && 0
            char buf[64];
            epicsTime ts = epicsTime::getCurrent();
            ts.strftime ( buf, sizeof(buf), "%M:%S.%09f");
#       endif
        // debugPrintf ( ( "Response set timer delay to zero. ts=%s\n", 
        //    buf ) );
        this->timer.start ( *this, currentTime );
    }
}

//
// searchTimer::expire ()
//
epicsTimerNotify::expireStatus searchTimer::expire ( const epicsTime & currentTime ) // X aCC 361
{
    // check to see if there is nothing to do here 
    if ( this->iiu.channelCount () == 0 ) {
        debugPrintf ( ( "all channels located - search timer terminating\n" ) );
        epicsGuard < searchTimerMutex > locker ( this->mutex );
        this->period = DBL_MAX;
        return noRestart;
    }   

    //
    // dynamically adjust the number of UDP frames per 
    // try depending how many search requests are not 
    // replied to
    //
    // The variable this->framesPerTry
    // determines the number of UDP frames to be sent
    // each time that expire() is called.
    // If this value is too high we will waste some
    // network bandwidth. If it is too low we will
    // use very little of the incoming UDP message
    // buffer associated with the server's port and
    // will therefore take longer to connect. We 
    // initialize this->framesPerTry to a prime number 
    // so that it is less likely that the
    // same channel is in the last UDP frame
    // sent every time that this is called (and
    // potentially discarded by a CA server with
    // a small UDP input queue). 
    //
    // increase frames per try only if we see better than
    // a 93.75% success rate for one pass through the list
    //
    if ( this->searchResponses >
        ( this->searchAttempts - (this->searchAttempts/16u) ) ) {
        // increase UDP frames per try if we have a good score
        if ( this->framesPerTry < maxTriesPerFrame ) {
            // a congestion avoidance threshold similar to TCP is now used
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
    // if we detect congestion because we have less than a 87.5% success 
    // rate then gradually reduce the frames per try
    else if ( this->searchResponses < 
        ( this->searchAttempts - (this->searchAttempts/8u) ) ) {
        if ( this->framesPerTry > 1 ) {
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

    this->dgSeqNoAtTimerExpireBegin = this->iiu.datagramSeqNumber ();

    this->searchAttempts = 0;
    this->searchResponses = 0;

    unsigned nChanSent = 0u;
    unsigned nFrameSent = 0u;
    while ( true ) {
            
        // check to see if we have reached the end of the list
        if ( this->searchAttemptsThisPass >= this->iiu.channelCount () ) {
            // if we are making some progress then dont increase the 
            // delay between search requests
            if ( this->searchResponsesThisPass == 0u ) {
                this->recomputeTimerPeriod ( this->minRetryThisPass );
            }
        
            this->minRetryThisPass = UINT_MAX;
                
            this->searchAttemptsThisPass = 0;
            this->searchResponsesThisPass = 0;

            debugPrintf ( ("saw end of list\n") );
        }
    
        unsigned retryNoForThisChannel;
        if ( ! this->iiu.searchMsg ( retryNoForThisChannel ) ) {
            nFrameSent++;
        
            if ( nFrameSent >= this->framesPerTry ) {
                break;
            }

            this->dgSeqNoAtTimerExpireEnd = this->iiu.datagramSeqNumber ();
            this->iiu.datagramFlush ( currentTime );
       
            if ( ! this->iiu.searchMsg ( retryNoForThisChannel ) ) {
                break;
            }
         }

        if (  this->minRetryThisPass > retryNoForThisChannel ) {
             this->minRetryThisPass = retryNoForThisChannel;
        }

        if ( this->searchAttempts < UINT_MAX ) {
            this->searchAttempts++;
        }
        if ( nChanSent < UINT_MAX ) {
            nChanSent++;
        }

        //
        // dont send any of the channels twice within one try
        //
       if ( nChanSent >= this->iiu.channelCount () ) {
            //
            // add one to nFrameSent because there may be 
            // one more partial frame to be sent
            //
            nFrameSent++;
        
            // 
            // cap this->framesPerTry to
            // the number of frames required for all of 
            // the unresolved channels
            //
            if ( this->framesPerTry > nFrameSent ) {
                this->framesPerTry = nFrameSent;
            }
        
            break;
        }
    }

    // flush out the search request buffer
    this->iiu.datagramFlush ( currentTime );

    this->dgSeqNoAtTimerExpireEnd = this->iiu.datagramSeqNumber () - 1u;

#   ifdef DEBUG
        char buf[64];
        epicsTime ts = epicsTime::getCurrent();
        ts.strftime ( buf, sizeof(buf), "%M:%S.%09f");
        debugPrintf ( ("sent %u delay sec=%f Rts=%s\n", 
            nFrameSent, this->period, buf ) );
#   endif

    if ( this->iiu.channelCount () == 0 ) {
        debugPrintf ( ( "all channels connected\n" ) );
        epicsGuard < searchTimerMutex > locker ( this->mutex );
        this->period = DBL_MAX;
        return noRestart;
    }
    else if ( this->minRetry < maxSearchTries ) {
        epicsGuard < searchTimerMutex > locker ( this->mutex );
        return expireStatus ( restart, this->period );
    }
    else {
        debugPrintf ( ( "maximum search tries exceeded - giving up\n" ) );
        epicsGuard < searchTimerMutex > locker ( this->mutex );
        this->period = DBL_MAX;
        return noRestart;
    }
}

void searchTimer::show ( unsigned /* level */ ) const
{
}

