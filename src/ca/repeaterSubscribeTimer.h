
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

#ifndef repeaterSubscribeTimerh  
#define repeaterSubscribeTimerh

#include "epicsTimer.h"

class udpiiu;

class repeaterSubscribeTimer : private epicsTimerNotify {
public:
    repeaterSubscribeTimer ( udpiiu &iiu, epicsTimerQueue &queue );
    virtual ~repeaterSubscribeTimer ();
    void confirmNotify ();
	void show (unsigned level) const;
private:
    epicsTimer &timer;
    udpiiu &iiu;
    unsigned attempts;
    bool registered;
    bool once;
	expireStatus expire ( const epicsTime & currentTime );
};

#endif // ifdef repeaterSubscribeTimerh
