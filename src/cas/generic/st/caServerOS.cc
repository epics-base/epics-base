
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

//
// casBeaconTimer
//
class casBeaconTimer : public osiTimer {
public:
    casBeaconTimer (double delay, caServerOS &osIn) :
        osiTimer(delay), os (osIn) {}
    void expire();
    double delay() const;
    bool again() const;
    const char *name() const;
private:
    caServerOS      &os;
};

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
	if (this->pBTmr) {
		delete this->pBTmr;
	}
}

//
// casBeaconTimer::expire()
//
void casBeaconTimer::expire()	
{
	os.sendBeacon ();
}

//
// casBeaconTimer::again()
//
bool casBeaconTimer::again()	const
{
	return true; 
}

//
// casBeaconTimer::delay()
//
double casBeaconTimer::delay() const
{
	return os.getBeaconPeriod();
}

//
// casBeaconTimer::name()
//
const char *casBeaconTimer::name() const
{
	return "casBeaconTimer";
}

