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
#include "netiiu.h"

class searchTimerNotify {
public:
    virtual ~searchTimerNotify () = 0;
    virtual void boostChannel ( 
        epicsGuard < epicsMutex > &, nciu & ) = 0;
    virtual void noSearchRespNotify ( 
        epicsGuard < epicsMutex > &, nciu &, unsigned ) = 0;
    virtual double getRTTE ( epicsGuard < epicsMutex > & ) const = 0;
    virtual void updateRTTE ( epicsGuard < epicsMutex > &, double rtte ) = 0;
    virtual bool datagramFlush ( 
        epicsGuard < epicsMutex > &, 
        const epicsTime & currentTime ) = 0;
    virtual ca_uint32_t datagramSeqNumber (
        epicsGuard < epicsMutex > & ) const = 0;
};

class searchTimer : private epicsTimerNotify {
public:
    searchTimer ( 
        class searchTimerNotify &, epicsTimerQueue &, 
        const unsigned index, epicsMutex &,
        bool boostPossible );
    virtual ~searchTimer ();
    void start ( epicsGuard < epicsMutex > & );
    void shutdown ( 
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void moveChannels ( 
        epicsGuard < epicsMutex > &, searchTimer & dest );
    void installChannel ( 
        epicsGuard < epicsMutex > &, nciu & );
    void uninstallChan ( 
        epicsGuard < epicsMutex > &, nciu & );
    void uninstallChanDueToSuccessfulSearchResponse ( 
        epicsGuard < epicsMutex > &, nciu &, 
        ca_uint32_t respDatagramSeqNo, bool seqNumberIsValid, 
        const epicsTime & currentTime );
    void show ( unsigned level ) const;
private:
    tsDLList < nciu > chanListReqPending;
    tsDLList < nciu > chanListRespPending;
    epicsTime timeAtLastSend;
    epicsTimer & timer;
    searchTimerNotify & iiu;
    epicsMutex & mutex;
    double framesPerTry; /* # of UDP frames per search try */
    double framesPerTryCongestThresh; /* one half N tries w congest */
    unsigned retry;
    unsigned searchAttempts; /* num search tries after last timer experation */
    unsigned searchResponses; /* num search resp after last timer experation */
    const unsigned index;
    ca_uint32_t dgSeqNoAtTimerExpireBegin; 
    ca_uint32_t dgSeqNoAtTimerExpireEnd;
    const bool boostPossible;
    bool stopped;

    expireStatus expire ( const epicsTime & currentTime );
    double period ( epicsGuard < epicsMutex > & ) const;
	searchTimer ( const searchTimer & ); // not implemented
	searchTimer & operator = ( const searchTimer & ); // not implemented
};

#endif // ifdef searchTimerh
