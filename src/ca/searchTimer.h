
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
    searchTimer ( udpiiu &iiu, epicsTimerQueue &queue, epicsMutex & );
    virtual ~searchTimer ();
    void notifySearchResponse ( unsigned short retrySeqNo );
    void resetPeriod ( double delayToNextTry );
    void show ( unsigned level ) const;
private:
    epicsTimer &timer;
    epicsMutex &mutex;
    udpiiu &iiu;
    unsigned framesPerTry; /* # of UDP frames per search try */
    unsigned framesPerTryCongestThresh; /* one half N tries w congest */
    unsigned minRetry; /* min retry number so far */
    unsigned retry;
    unsigned searchTriesWithinThisPass; /* num search tries within this pass through the list */
    unsigned searchResponsesWithinThisPass; /* num search resp within this pass through the list */
    unsigned short retrySeqNo; /* search retry seq number */
    unsigned short retrySeqAtPassBegin; /* search retry seq number at beg of pass through list */
    double period; /* period between tries */
    expireStatus expire ();
    void setRetryInterval (unsigned retryNo);
};

#endif // ifdef searchTimerh
