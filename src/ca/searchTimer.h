
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef searchTimerh  
#define searchTimerh

#include "epicsTimer.h"
#include "epicsMutex.h"

class udpiiu;

class searchTimer : private epicsTimerNotify {
public:
    searchTimer ( udpiiu &, epicsTimerQueue &, epicsMutex & );
    virtual ~searchTimer ();
    void notifySearchResponse ( unsigned short retrySeqNo, const epicsTime & currentTime );
    void resetPeriod ( double delayToNextTry );
    void show ( unsigned level ) const;
private:
    epicsTime timeAtLastRetry;
    double period; /* period between tries */
    double roundTripDelayEstimate;
    epicsTimer &timer;
    epicsMutex &mutex;
    udpiiu &iiu;
    unsigned framesPerTry; /* # of UDP frames per search try */
    unsigned framesPerTryCongestThresh; /* one half N tries w congest */
    unsigned minRetry; /* min retry number so far */
    unsigned retry;
    unsigned searchAttempts; /* num search tries within this timer experation */
    unsigned searchResponses; /* num search resp within this timer experation */
    unsigned searchAttemptsThisPass; /* num search tries within this pass */
    unsigned searchResponsesThisPass; /* num search resp within this pass */
    unsigned short retrySeqNo; /* search retry seq number */
    unsigned short retrySeqAtPassBegin; /* search retry seq number at beg of pass through list */
    bool active;
    bool noDelay;
    expireStatus expire ( const epicsTime & currentTime );
    void setRetryInterval ( unsigned retryNo );
	searchTimer ( const searchTimer & );
	searchTimer & operator = ( const searchTimer & );
};

#endif // ifdef searchTimerh
