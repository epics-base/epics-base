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

#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsTimer.h"

#ifdef searchTimerh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "caProto.h"

class udpMutex;

class searchTimer : private epicsTimerNotify {
public:
    searchTimer ( class udpiiu &, epicsTimerQueue &, udpMutex & );
    virtual ~searchTimer ();
    void notifySuccessfulSearchResponse ( epicsGuard < udpMutex > &, 
        ca_uint32_t respDatagramSeqNo, 
        bool seqNumberIsValid, const epicsTime & currentTime );
    void beaconAnomalyNotify ( epicsGuard < udpMutex > &,
        const epicsTime & currentTime, const double & delay );
    void channelCreatedNotify ( epicsGuard < udpMutex > &,
        const epicsTime &, bool firstChannel );
    void channelDisconnectedNotify ( epicsGuard < udpMutex > &,
        const epicsTime &, bool firstChannel );
    void shutdown ();
    void show ( unsigned level ) const;
private:
    double period; /* period between tries */
    epicsTimer & timer;
    class udpiiu & iiu;
    udpMutex & mutex;
    double framesPerTry; /* # of UDP frames per search try */
    double framesPerTryCongestThresh; /* one half N tries w congest */
    double maxPeriod;
    unsigned retry;
    unsigned searchAttempts; /* num search tries after last timer experation */
    unsigned searchResponses; /* num search resp after last timer experation */
    unsigned searchAttemptsThisPass; /* num search tries within this pass */
    unsigned searchResponsesThisPass; /* num search resp within this pass */
    ca_uint32_t dgSeqNoAtTimerExpireBegin; 
    ca_uint32_t dgSeqNoAtTimerExpireEnd;
    bool stopped;
    expireStatus expire ( const epicsTime & currentTime );
    void recomputeTimerPeriod ( epicsGuard < udpMutex > &, const unsigned minRetryNew );
    void recomputeTimerPeriodAndStartTimer ( epicsGuard < udpMutex > &,
        const epicsTime & currentTime, const unsigned minRetryNew, 
        const double & initialDelay );
    void newChannelNotify ( epicsGuard < udpMutex > &,
        const epicsTime &, bool firstChannel,
        const unsigned minRetryNo );
	searchTimer ( const searchTimer & );
	searchTimer & operator = ( const searchTimer & );
};

#endif // ifdef searchTimerh
