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
//
//                    L O S  A L A M O S
//              Los Alamos National Laboratory
//               Los Alamos, New Mexico 87545
//
//  Copyright, 1986, The Regents of the University of California.
//
//  Author: Jeff Hill
//

#include <stdexcept>
#include <string> // vxWorks 6.0 requires this include 
#include <limits.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "tsMinMax.h"
#include "envDefs.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "udpiiu.h"
#include "nciu.h"

static const unsigned initialTriesPerFrame = 1u; // initial UDP frames per search try
static const unsigned maxTriesPerFrame = 64u; // max UDP frames per search try 

//
// searchTimer::searchTimer ()
//
searchTimer::searchTimer ( 
        searchTimerNotify & iiuIn, 
        epicsTimerQueue & queueIn, 
        const unsigned indexIn, 
        epicsMutex & mutexIn,
        bool boostPossibleIn ) :
    timeAtLastSend ( epicsTime::getCurrent () ),
    timer ( queueIn.createTimer () ),
    iiu ( iiuIn ),
    mutex ( mutexIn ),
    framesPerTry ( initialTriesPerFrame ),
    framesPerTryCongestThresh ( DBL_MAX ),
    retry ( 0 ),
    searchAttempts ( 0u ),
    searchResponses ( 0u ),
    index ( indexIn ),
    dgSeqNoAtTimerExpireBegin ( 0u ),
    dgSeqNoAtTimerExpireEnd ( 0u ),
    boostPossible ( boostPossibleIn ),
    stopped ( false )
{
}

void searchTimer::start ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->timer.start ( *this, this->period ( guard ) );
}

searchTimer::~searchTimer ()
{
    assert ( this->chanListReqPending.count() == 0 );
    assert ( this->chanListRespPending.count() == 0 );
    this->timer.destroy ();
}

void searchTimer::shutdown ( 
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    this->stopped = true;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        {
            epicsGuardRelease < epicsMutex > cbUnguard ( cbGuard );
            this->timer.cancel ();
        }
    }

    while ( nciu * pChan = this->chanListReqPending.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }
    while ( nciu * pChan = this->chanListRespPending.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }
}

void searchTimer::installChannel ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    this->chanListReqPending.add ( chan );
    chan.channelNode::setReqPendingState ( guard, this->index );
}

void searchTimer::moveChannels ( 
    epicsGuard < epicsMutex > & guard, searchTimer & dest )
{
    while ( nciu * pChan = this->chanListRespPending.get () ) {
        if ( this->searchAttempts > 0 ) {
            this->searchAttempts--;
        }
        dest.installChannel ( guard, *pChan );
    }
    while ( nciu * pChan = this->chanListReqPending.get () ) {
        dest.installChannel ( guard, *pChan );
    }
}

//
// searchTimer::expire ()
//
epicsTimerNotify::expireStatus searchTimer::expire ( 
    const epicsTime & currentTime )
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    while ( nciu * pChan = this->chanListRespPending.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        this->iiu.noSearchRespNotify ( 
            guard, *pChan, this->index );
    }
    
    this->timeAtLastSend = currentTime;

    // boost search period for channels not recently
    // searched for if there was some success
    if ( this->searchResponses && this->boostPossible ) {
        while ( nciu * pChan = this->chanListReqPending.get () ) {
            pChan->channelNode::listMember = 
                channelNode::cs_none;
            this->iiu.boostChannel ( guard, *pChan );
        }
    }

    if ( this->searchAttempts ) {
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
    }

    this->dgSeqNoAtTimerExpireBegin = 
        this->iiu.datagramSeqNumber ( guard );

    this->searchAttempts = 0;
    this->searchResponses = 0;

    unsigned nFrameSent = 0u;
    while ( true ) {
        nciu * pChan = this->chanListReqPending.get ();
        if ( ! pChan ) {
            break;
        }

        pChan->channelNode::listMember = 
            channelNode::cs_none;
    
        bool success = pChan->searchMsg ( guard );
        if ( ! success ) {
            if ( this->iiu.datagramFlush ( guard, currentTime ) ) {
                nFrameSent++;
                if ( nFrameSent < this->framesPerTry ) {
                    success = pChan->searchMsg ( guard );
                }
            }
            if ( ! success ) {
                this->chanListReqPending.push ( *pChan );
                pChan->channelNode::setReqPendingState ( 
                    guard, this->index );
                break;
            }
        }

        this->chanListRespPending.add ( *pChan );
        pChan->channelNode::setRespPendingState ( 
            guard, this->index );

        if ( this->searchAttempts < UINT_MAX ) {
            this->searchAttempts++;
        }
    }

    // flush out the search request buffer
    if ( this->iiu.datagramFlush ( guard, currentTime ) ) {
        nFrameSent++;
    }

    this->dgSeqNoAtTimerExpireEnd = 
        this->iiu.datagramSeqNumber ( guard ) - 1u;

#   ifdef DEBUG
        if ( this->searchAttempts ) {
            char buf[64];
            currentTime.strftime ( buf, sizeof(buf), "%M:%S.%09f");
            debugPrintf ( ("sent %u delay sec=%f Rts=%s\n", 
                nFrameSent, this->period(), buf ) );
        }
#   endif

    return expireStatus ( restart, this->period ( guard ) );
}

void searchTimer :: show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    ::printf ( "searchTimer with period %f\n", this->period ( guard ) );
    if ( level > 0 ) {
        ::printf ( "channels with search request pending = %u\n", 
            this->chanListReqPending.count () );
        if ( level > 1u ) {
            tsDLIterConst < nciu > pChan = 
                this->chanListReqPending.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        ::printf ( "channels with search response pending = %u\n", 
            this->chanListRespPending.count () );
        if ( level > 1u ) {
            tsDLIterConst < nciu > pChan = 
                this->chanListRespPending.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
    }
}

//
// Reset the delay to the next search request if we get
// at least one response. However, dont reset this delay if we
// get a delayed response to an old search request.
//
void searchTimer::uninstallChanDueToSuccessfulSearchResponse ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    ca_uint32_t respDatagramSeqNo, bool seqNumberIsValid, 
    const epicsTime & currentTime )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->uninstallChan ( guard, chan );

    if ( this->stopped ) {
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
        double measured = currentTime - this->timeAtLastSend;
        this->iiu.updateRTTE ( guard, measured );

        if ( this->searchResponses < UINT_MAX ) {
            this->searchResponses++;
            if ( this->searchResponses == this->searchAttempts ) {
                if ( this->chanListReqPending.count () ) {
                    //
                    // when we get 100% success immediately 
                    // send another search request
                    //
                    debugPrintf ( ( "All requests succesful, set timer delay to zero\n" ) );
                    this->timer.start ( *this, currentTime );
                }
            }
        }
    }
}

void searchTimer::uninstallChan (
    epicsGuard < epicsMutex > & cacGuard, nciu & chan )
{
    cacGuard.assertIdenticalMutex ( this->mutex );
    unsigned ulistmem = 
	static_cast <unsigned> ( chan.channelNode::listMember );
    unsigned uReqBase = 
	static_cast <unsigned> ( channelNode::cs_searchReqPending0 );
    if ( ulistmem == this->index + uReqBase ) {
        this->chanListReqPending.remove ( chan );
    }
    else { 
	unsigned uRespBase = 
	    static_cast <unsigned > ( 
		channelNode::cs_searchRespPending0 );
	if ( ulistmem == this->index + uRespBase ) {
	    this->chanListRespPending.remove ( chan );
	}
	else {
	    throw std::runtime_error ( 
		"uninstalling channel search timer, but channel "
		"state is wrong" );
	}
    }
    chan.channelNode::listMember = channelNode::cs_none;
}

double searchTimer::period (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return (1 << this->index ) * this->iiu.getRTTE ( guard );
}

searchTimerNotify::~searchTimerNotify () {}
