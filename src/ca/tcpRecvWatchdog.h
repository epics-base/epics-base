
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

#ifndef tcpRecvWatchdogh  
#define tcpRecvWatchdogh

#include "epicsTimer.h"

class tcpiiu;

class tcpRecvWatchdog : private epicsTimerNotify {
public:
    tcpRecvWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & );
    virtual ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void sendBacklogProgressNotify ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();
    void cancel ();
    void show ( unsigned level ) const;
private:
    const double period;
    epicsTimer & timer;
    tcpiiu &iiu;
    bool responsePending;
    bool beaconAnomaly;
    expireStatus expire ( const epicsTime & currentTime );
};

#endif // #ifndef tcpRecvWatchdogh

