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
#include "envDefs.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "searchTimer.h"
#include "udpiiu.h"

static const unsigned initialTriesPerFrame = 1u; // initial UDP frames per search try
static const unsigned maxTriesPerFrame = 64u; // max UDP frames per search try 

static const double minSearchPeriod = 30e-3; // seconds
static const double maxSearchPeriodDefault = 5.0 * 60.0; // seconds
static const double maxSearchPeriodLowerLimit = 60.0; // seconds

// This impacts the exponential backoff delay between search messages.
// This delay is two to the power of the minimum channel retry count
// times the estimated round trip time or the OS's delay quantum 
// whichever is greater. So this results in about a one second delay. 
static const unsigned successfulSearchRetrySetpoint = 6u;
static const unsigned beaconAnomalyRetrySetpoint = 6u;
static const unsigned disconnectRetrySetpoint = 6u;

//
// searchTimer::searchTimer ()
//
searchTimer::searchTimer ( udpiiu & iiuIn, 
        epicsTimerQueue & queueIn, udpMutex & mutexIn ) :
    period ( 1e9 ),
    timer ( queueIn.createTimer () ),
    iiu ( iiuIn ),
    mutex ( mutexIn ),
    framesPerTry ( initialTriesPerFrame ),
    framesPerTryCongestThresh ( DBL_MAX ),
    maxPeriod ( maxSearchPeriodDefault ),
    retry ( 0 ),
    searchAttempts ( 0u ),
    searchResponses ( 0u ),
    searchAttemptsThisPass ( 0u ),
    searchResponsesThisPass ( 0u ), 
    dgSeqNoAtTimerExpireBegin ( 0u ),
    dgSeqNoAtTimerExpireEnd ( 0u ),
    stopped ( false )
{
    if ( envGetConfigParamPtr ( & EPICS_CA_MAX_SEARCH_PERIOD ) ) {
        long longStatus = envGetDoubleConfigParam ( 
            & EPICS_CA_MAX_SEARCH_PERIOD, & this->maxPeriod );
        if ( ! longStatus ) {
            if ( this->maxPeriod < maxSearchPeriodLowerLimit ) {
                epicsPrintf ( "EPICS \"%s\" out of range (low)\n",
                                EPICS_CA_MAX_SEARCH_PERIOD.name );
                this->maxPeriod = maxSearchPeriodLowerLimit;
                epicsPrintf ( "Setting \"%s\" = %f seconds\n",
                    EPICS_CA_MAX_SEARCH_PERIOD.name, this->maxPeriod );
            }
        }
        else {
            epicsPrintf ( "EPICS \"%s\" wasnt a real number\n",
                            EPICS_CA_MAX_SEARCH_PERIOD.name );
            epicsPrintf ( "Setting \"%s\" = %f seconds\n",
                EPICS_CA_MAX_SEARCH_PERIOD.name, this->maxPeriod );
        }
    }
}

searchTimer::~searchTimer ()
{
    this->timer.destroy ();
}

void searchTimer::shutdown ()
{
    this->stopped = true;
    this->timer.cancel ();
}

void searchTimer::channelCreatedNotify ( 
    epicsGuard < udpMutex > & guard,
    const epicsTime & currentTime, bool firstChannel )
{
    this->newChannelNotify ( guard, currentTime, 
        firstChannel, 0 );
}

void searchTimer::channelDisconnectedNotify ( 
    epicsGuard < udpMutex > & guard,
    const epicsTime & currentTime, bool firstChannel )
{
    this->newChannelNotify ( guard, currentTime, 
        firstChannel, disconnectRetrySetpoint );
}

void searchTimer::newChannelNotify ( 
    epicsGuard < udpMutex > & guard, const epicsTime & currentTime, 
    bool firstChannel, const unsigned minRetryNo )
{
    if ( ! this->stopped ) {
        if ( firstChannel ) {
            this->recomputeTimerPeriod ( guard, minRetryNo );
            double newPeriod = this->period;
            {
                // avoid timer cancel block deadlock
                epicsGuardRelease < udpMutex > unguard ( guard );
                this->timer.start ( *this, currentTime + newPeriod );
            }
        }
        else {
            this->recomputeTimerPeriodAndStartTimer ( guard, 
                currentTime, minRetryNo, 0.0 );
        }
    }
}

void searchTimer::beaconAnomalyNotify ( 
    epicsGuard < udpMutex > & guard, 
    const epicsTime & currentTime, const double & delay )
{
    this->recomputeTimerPeriodAndStartTimer ( 
        guard, currentTime, beaconAnomalyRetrySetpoint, delay );
}

// lock must be applied
void searchTimer::recomputeTimerPeriod ( 
    epicsGuard < udpMutex > & guard, const unsigned retryNew ) // X aCC 431
{
    this->retry = retryNew;
    unsigned idelay = 1u << tsMin ( this->retry, CHAR_BIT * sizeof ( idelay ) - 1u );
    double delayFactor = tsMax ( 
        this->iiu.roundTripDelayEstimate ( guard ) * 2.0, minSearchPeriod );
    this->period = idelay * delayFactor; /* sec */ 
    this->period = tsMin ( this->maxPeriod, this->period );
}

void searchTimer::recomputeTimerPeriodAndStartTimer ( epicsGuard < udpMutex > & guard,
    const epicsTime & currentTime, const unsigned retryNew, const double & initialDelay )
{
    if ( this->iiu.unresolvedChannelCount ( guard ) == 0 || this->stopped ) {
        return; 
    }

    bool start = false;
    double totalDelay = initialDelay;
    {
        if ( this->retry <= retryNew  ) {
            return; 
        }

        double oldPeriod = this->period;

        this->recomputeTimerPeriod ( guard, retryNew );

        totalDelay += this->period;

        if ( totalDelay < oldPeriod ) {
            epicsTimer::expireInfo info = this->timer.getExpireInfo ();
            if ( info.active ) {
                double delay = info.expireTime - currentTime;
                if ( delay > totalDelay ) {
                    start = true;
                }
            }
            else {
                start = true;
            }
        }
    }

    if ( start ) {
        // avoid timer cancel block deadlock
        epicsGuardRelease < udpMutex > unguard ( guard );
        this->timer.start ( *this, currentTime + totalDelay );
    }
    debugPrintf ( ( "changed search period to %f sec\n", this->period ) );
}

//
// searchTimer::notifySuccessfulSearchResponse ()
//
// Reset the delay to the next search request if we get
// at least one response. However, dont reset this delay if we
// get a delayed response to an old search request.
//
void searchTimer::notifySuccessfulSearchResponse ( epicsGuard < udpMutex > & guard,
    ca_uint32_t respDatagramSeqNo, bool seqNumberIsValid, const epicsTime & currentTime )
{
    if ( this->iiu.unresolvedChannelCount ( guard ) == 0 || this->stopped ) {
        return;
    }

    bool validResponse = true;
    if ( seqNumberIsValid ) {
        validResponse = 
            this->dgSeqNoAtTimerExpireBegin <= respDatagramSeqNo && 
                this->dgSeqNoAtTimerExpireEnd >= respDatagramSeqNo;
    }

    // if we receive a successful response then reset to a
    // reasonable timer period
    if ( validResponse ) {
        if ( this->searchResponses < UINT_MAX ) {
            this->searchResponses++;
            if ( this->searchResponses == this->searchAttempts ) {
                debugPrintf ( ( "Response set timer delay to zero\n" ) );
                if ( this->retry > successfulSearchRetrySetpoint ) {
                    this->recomputeTimerPeriod ( 
                        guard, successfulSearchRetrySetpoint );
                }
                // avoid timer cancel block deadlock
                epicsGuardRelease < udpMutex > unguard ( guard );
                //
                // when we get 100% success immediately 
                // send another search request
                //
                this->timer.start ( *this, currentTime );
            }
            else {
                debugPrintf ( ( "Response set timer delay to beacon anomaly set point\n" ) );
                //
                // otherwise, if making some progress then dont allow
                // retry rate to drop below some reasonable minimum
                //
                if ( this->retry > successfulSearchRetrySetpoint ) {
                    this->recomputeTimerPeriodAndStartTimer ( 
                        guard, currentTime, successfulSearchRetrySetpoint, 0.0 );
                }
            }
        }
    }
}

//
// searchTimer::expire ()
//
epicsTimerNotify::expireStatus searchTimer::expire ( const epicsTime & currentTime ) // X aCC 361
{
    epicsGuard < udpMutex > guard ( this->mutex );

    // check to see if there is nothing to do here 
    if ( this->iiu.unresolvedChannelCount ( guard ) == 0 ) {
        debugPrintf ( ( "all channels located - search timer terminating\n" ) );
        this->period = DBL_MAX;
        return noRestart;
    }   

#if 0
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
        debugPrintf ( ("Congestion detected - set frames per try to %f t=%u r=%u\n", 
            this->framesPerTry, this->searchAttempts, this->searchResponses) );
    }
#else
    if ( this->searchResponses == this->searchAttempts ) {
        // increase UDP frames per try if we have a good score
        if ( this->framesPerTry < maxTriesPerFrame ) {
            // a congestion avoidance threshold similar to TCP is now used
            if ( this->framesPerTry < this->framesPerTryCongestThresh ) {
                double doubled = 2 * this->framesPerTry;
                if ( doubled > this->framesPerTryCongestThresh ) {
                    this->framesPerTry = this->framesPerTryCongestThresh;
                }
                else {
                    this->framesPerTry = doubled;
                }
            }
            else {
                this->framesPerTry += 1.0 / this->framesPerTry;
            }
            debugPrintf ( ("Increasing frame count to %g t=%u r=%u\n", 
                this->framesPerTry, this->searchAttempts, this->searchResponses) );
        }
    }
    else  {
        this->framesPerTryCongestThresh = this->framesPerTry / 2.0;
        this->framesPerTry = 1u;
        debugPrintf ( ("Congestion detected - set frames per try to %g t=%u r=%u\n", 
            this->framesPerTry, this->searchAttempts, this->searchResponses) );
    }
#endif

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

    this->dgSeqNoAtTimerExpireBegin = this->iiu.datagramSeqNumber ( guard );

    this->searchAttempts = 0;
    this->searchResponses = 0;

    unsigned nChanSent = 0u;
    unsigned nFrameSent = 0u;
    while ( true ) {
            
        // check to see if we have reached the end of the list
        if ( this->searchAttemptsThisPass >= this->iiu.unresolvedChannelCount ( guard ) ) {
            // if we are making some progress then dont increase the 
            // delay between search requests
            if ( this->searchResponsesThisPass == 0u ) {
                this->recomputeTimerPeriod ( guard, this->retry + 1 );
            }
                
            this->searchAttemptsThisPass = 0;
            this->searchResponsesThisPass = 0;

            debugPrintf ( ("saw end of list\n") );
        }
    
        if ( ! this->iiu.searchMsg ( guard ) ) {
            nFrameSent++;
        
            if ( nFrameSent >= this->framesPerTry ) {
                break;
            }
            this->dgSeqNoAtTimerExpireEnd = this->iiu.datagramSeqNumber ( guard );
            this->iiu.datagramFlush ( guard, currentTime );
            if ( ! this->iiu.searchMsg ( guard ) ) {
                break;
            }
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
       if ( nChanSent >= this->iiu.unresolvedChannelCount ( guard ) ) {
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
    this->iiu.datagramFlush ( guard, currentTime );

    this->dgSeqNoAtTimerExpireEnd = this->iiu.datagramSeqNumber ( guard ) - 1u;

#   ifdef DEBUG
        char buf[64];
        epicsTime ts = currentTime;
        ts.strftime ( buf, sizeof(buf), "%M:%S.%09f");
        debugPrintf ( ("sent %u delay sec=%f Rts=%s\n", 
            nFrameSent, this->period, buf ) );
#   endif

    if ( this->iiu.unresolvedChannelCount ( guard ) == 0 ) {
        debugPrintf ( ( "all channels connected\n" ) );
        this->period = DBL_MAX;
        return noRestart;
    }

    // the code used to test this->minRetry < maxSearchTries here 
    // and return no restart if the maximum tries was exceeded
    // prior to R3.14.7
    return expireStatus ( restart, this->period );
}

void searchTimer::show ( unsigned /* level */ ) const
{
}

