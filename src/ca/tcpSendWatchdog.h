
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

#ifndef tcpSendWatchdogh  
#define tcpSendWatchdogh

#include "epicsTimer.h"

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ();
    void cancel ();
private:
    const double period;
    epicsTimer & timer;
    tcpiiu & iiu;
    expireStatus expire ( const epicsTime & currentTime );
};

#endif // #ifndef tcpSendWatchdog
