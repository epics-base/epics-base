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

#ifndef disconnectGovernorTimerh  
#define disconnectGovernorTimerh

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

class disconnectGovernorNotify {
public:
    virtual ~disconnectGovernorNotify () = 0;
    virtual void govExpireNotify ( 
        epicsGuard < epicsMutex > &, nciu & ) = 0;
};

class disconnectGovernorTimer : private epicsTimerNotify {
public:
    disconnectGovernorTimer ( 
        class disconnectGovernorNotify &, epicsTimerQueue &, epicsMutex & );
    virtual ~disconnectGovernorTimer ();
    void start ();
    void shutdown (
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void installChan ( 
        epicsGuard < epicsMutex > &, nciu & );
    void uninstallChan ( 
        epicsGuard < epicsMutex > &, nciu & );
    void show ( unsigned level ) const;
private:
    tsDLList < nciu > chanList;
    epicsMutex & mutex;
    epicsTimer & timer;
    class disconnectGovernorNotify & iiu;
    epicsTimerNotify::expireStatus expire ( const epicsTime & currentTime );
	disconnectGovernorTimer ( const disconnectGovernorTimer & );
	disconnectGovernorTimer & operator = ( const disconnectGovernorTimer & );
};

#endif // ifdef disconnectGovernorTimerh
