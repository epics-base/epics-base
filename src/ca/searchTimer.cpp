
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

#include "iocinf.h"

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

//
// searchTimer::searchTimer ()
//
searchTimer::searchTimer (udpiiu &iiuIn, osiTimerQueue &queueIn) :
    osiTimer (queueIn),
    iiu (iiuIn),
    framesPerTry (INITIALTRIESPERFRAME),
    framesPerTryCongestThresh (UINT_MAX),
    minRetry (UINT_MAX),
    retry (0u),
    searchTriesWithinThisPass (0u),
    searchResponsesWithinThisPass (0u),
    retrySeqNo (0u),
    retrySeqAtPassBegin (0u),
    period (CA_RECAST_DELAY)
{
}

//
// searchTimer::reset ()
//
void searchTimer::reset ( double delayToNextTry )
{
    bool reschedule;

    if ( delayToNextTry < CA_RECAST_DELAY ) {
        delayToNextTry = CA_RECAST_DELAY;
    }

    this->lock ();
    this->retry = 0;
    if ( this->period > delayToNextTry ) {
        reschedule = true;
    }
    else {
        reschedule = false;
    }
    this->period = CA_RECAST_DELAY;
    this->unlock ();

    if ( reschedule ) {
        this->reschedule ( delayToNextTry );
        debugPrintf ( ("rescheduled search timer for completion in %f sec\n", delayToNextTry) );
    }
    else {
        this->activate ( delayToNextTry );
        debugPrintf ( ("if inactive, search timer started to completion in %f sec\n", delayToNextTry) );
    }
}

/* 
 * searchTimer::setRetryInterval ()
 */
void searchTimer::setRetryInterval (unsigned retryNo)
{
    unsigned idelay;
    double delay;

    this->lock ();

    /*
     * set the retry number
     */
    this->retry = min (retryNo, MAXCONNTRIES+1u);

    /*
     * set the retry interval
     */
    idelay = 1u << min (this->retry, CHAR_BIT*sizeof(idelay)-1u);
    delay = idelay * CA_RECAST_DELAY; /* sec */ 
    /*
     * place upper limit on the retry delay
     */
    this->period = min (CA_RECAST_PERIOD, delay);

    this->unlock ();

    debugPrintf ( ("new CA search period is %f sec\n", this->period) );
}

//
// searchTimer::notifySearchResponse ()
//
// Reset the delay to the next search request if we get
// at least one response. However, dont reset this delay if we
// get a delayed response to an old search request.
//
void searchTimer::notifySearchResponse ( unsigned short retrySeqNoIn )
{
    bool reschedualNeeded;

    this->lock ();

    if ( this->retrySeqAtPassBegin <= retrySeqNoIn ) {
        if ( this->searchResponsesWithinThisPass < UINT_MAX ) {
            this->searchResponsesWithinThisPass++;
        }
    }    

    reschedualNeeded = ( retrySeqNoIn == this->retrySeqNo );
        
    this->unlock ();

    if ( reschedualNeeded ) {
        this->reschedule (0.0);
    }
}

//
// searchTimer::expire ()
//
void searchTimer::expire ()
{
    unsigned nFrameSent = 0u;
    unsigned nChanSent = 0u;

    /*
     * check to see if there is nothing to do here 
     */
    if ( this->iiu.channelCount () == 0 ) {
        return;
    }   
    
    this->lock ();
 
    /*
     * increment the retry sequence number
     */
    this->retrySeqNo++; /* allowed to roll over */
    
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
    if (this->searchResponsesWithinThisPass >
        (this->searchTriesWithinThisPass-(this->searchTriesWithinThisPass/16u)) ) {
        /*
         * increase UDP frames per try if we have a good score
         */
        if ( this->framesPerTry < MAXTRIESPERFRAME ) {
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
                this->framesPerTry, this->searchTriesWithinThisPass, this->searchResponsesWithinThisPass) );
        }
    }
    /*
     * if we detect congestion because we have less than a 87.5% success 
     * rate then gradually reduce the frames per try
     */
    else if ( this->searchResponsesWithinThisPass < 
        (this->searchTriesWithinThisPass-(this->searchTriesWithinThisPass/8u)) ) {
            if (this->framesPerTry>1) {
                this->framesPerTry--;
            }
            this->framesPerTryCongestThresh = this->framesPerTry/2 + 1;
            debugPrintf ( ("Congestion detected - set frames per try to %u t=%u r=%u\n", 
                this->framesPerTry, this->searchTriesWithinThisPass, 
                this->searchResponsesWithinThisPass) );
    }
    
    while ( 1 ) {
                
        /*
         * clear counter when we reach the end of the list
         *
         * if we are making some progress then
         * dont increase the delay between search
         * requests
         */
        if ( this->searchTriesWithinThisPass >= this->iiu.channelCount () ) {
            if ( this->searchResponsesWithinThisPass == 0u ) {
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

            this->searchTriesWithinThisPass = 0;
            this->searchResponsesWithinThisPass = 0;

            debugPrintf ( ("saw end of list\n") );
        }
        
        unsigned retryNoForThisChannel;
        if ( ! this->iiu.searchMsg ( this->retrySeqNo, retryNoForThisChannel ) ) {
            nFrameSent++;
            
            if ( nFrameSent >= this->framesPerTry ) {
                break;
            }

            this->iiu.flush ();
           
            if ( ! this->iiu.searchMsg ( this->retrySeqNo, retryNoForThisChannel ) ) {
                break;
            }
        }

        this->minRetry = min ( this->minRetry, retryNoForThisChannel );

        if ( this->searchTriesWithinThisPass < UINT_MAX ) {
            this->searchTriesWithinThisPass++;
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
    
    this->unlock ();

    // flush out the search request buffer
    this->iiu.flush ();

    debugPrintf ( ("sent %u delay sec=%f\n", nFrameSent, this->period) );
}

void searchTimer::destroy ()
{
}

bool searchTimer::again () const
{
    if ( this->iiu.channelCount () == 0 ) {
        return false;
    }
    else {
        if ( this->retry < MAXCONNTRIES ) {
            return true;
        }
        else {
            return false;
        }
    }
}

double searchTimer::delay () const
{
    return this->period;
}

void searchTimer::show (unsigned /* level */) const
{
}

const char *searchTimer::name () const
{
    return "CAC Search Timer";
}
