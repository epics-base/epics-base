
//  
//  $Id$
//
//                              
//                    L O S  A L A M O S
//              Los Alamos National Laboratory
//               Los Alamos, New Mexico 87545
//                                  
//  Copyright, 1986, The Regents of the University of California.
//                                  
//           
//	Author Jeffrey O. Hill
//	johill@lanl.gov
//	505 665 1831
//

#ifndef searchTimerh  
#define searchTimerh

#ifdef epicsExportSharedSymbols
#   define searchTimerh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "epicsMutex.h"
#include "epicsTimer.h"

#ifdef searchTimerh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "caProto.h"

class searchTimerMutex : public epicsMutex {};

class searchTimer : private epicsTimerNotify {
public:
    searchTimer ( class udpiiu &, epicsTimerQueue & );
    virtual ~searchTimer ();
    void notifySearchResponse ( ca_uint32_t respDatagramSeqNo, 
        bool seqNumberIsValid, const epicsTime & currentTime );
    void newChannleNotify ( bool firstChannel );
    void beaconAnomalyNotify ( const double & delay );
    void show ( unsigned level ) const;
private:
    class searchTimerMutex mutex;
    double period; /* period between tries */
    epicsTimer & timer;
    class udpiiu & iiu;
    unsigned framesPerTry; /* # of UDP frames per search try */
    unsigned framesPerTryCongestThresh; /* one half N tries w congest */
    unsigned minRetry; /* min retry number so far */
    unsigned minRetryThisPass;
    unsigned searchAttempts; /* num search tries within this timer experation */
    unsigned searchResponses; /* num search resp within this timer experation */
    unsigned searchAttemptsThisPass; /* num search tries within this pass */
    unsigned searchResponsesThisPass; /* num search resp within this pass */
    ca_uint32_t dgSeqNoAtTimerExpireBegin; 
    ca_uint32_t dgSeqNoAtTimerExpireEnd;
    expireStatus expire ( const epicsTime & currentTime );
    void recomputeTimerPeriod ( unsigned minRetryNew );
    void recomputeTimerPeriodAndStartTimer ( unsigned minRetryNew, const double & initialDelay );
	searchTimer ( const searchTimer & );
	searchTimer & operator = ( const searchTimer & );
};

#endif // ifdef searchTimerh
