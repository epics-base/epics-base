
/*
 *
 * caServerOS.c
 * $Id$
 *
 */


//
// CA server
// 
#include "server.h"
#include "fdManager.h"

//
// casBeaconTimer
//
class casBeaconTimer : public epicsTimerNotify {
public:
    casBeaconTimer ( double delay, caServerOS &osIn );
    virtual ~casBeaconTimer ();
private:
    epicsTimer  &timer;
    caServerOS  &os;
    expireStatus expire ();
};

casBeaconTimer::casBeaconTimer ( double delay, caServerOS &osIn ) :
    timer ( fileDescriptorManager.timerQueueRef().createTimer(*this) ), os ( osIn ) 
{
    this->timer.start ( delay );
}

casBeaconTimer::~casBeaconTimer ()
{
    delete & this->timer;
}

//
// caServerOS::caServerOS()
//
caServerOS::caServerOS ()
{
	this->pBTmr = new casBeaconTimer(0.0, *this);
	if (!this->pBTmr) {
		throw S_cas_noMemory;
	}
}

//
// caServerOS::~caServerOS()
//
caServerOS::~caServerOS()
{
	if ( this->pBTmr ) {
		delete this->pBTmr;
	}
}

//
// casBeaconTimer::expire()
//
epicsTimerNotify::expireStatus casBeaconTimer::expire()	
{
	os.sendBeacon ();
    return expireStatus ( restart, os.getBeaconPeriod() );
}

