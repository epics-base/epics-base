
/*
 *
 * caServerOS.c
 * $Id$
 *
 *
 * $Log$
 * Revision 1.2  1997/04/10 19:34:28  jhill
 * API changes
 *
 * Revision 1.1  1996/11/02 01:01:27  jhill
 * installed
 *
 * Revision 1.2  1996/08/05 19:28:49  jhill
 * space became tab
 *
 * Revision 1.1.1.1  1996/06/20 00:28:06  jhill
 * ca server installation
 *
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
// aServerOS::operator -> ()
//
inline caServerI * caServerOS::operator -> ()
{
	return &this->cas;
}

//
// casBeaconTimer::expire()
//
void casBeaconTimer::expire()	
{
	os->sendBeacon ();
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
	return os->getBeaconPeriod();
}

//
// casBeaconTimer::name()
//
const char *casBeaconTimer::name() const
{
	return "casBeaconTimer";
}

//
// caServerOS::init()
//
caStatus caServerOS::init()
{
	this->pBTmr = new casBeaconTimer((*this)->getBeaconPeriod(), *this);
	if (!this->pBTmr) {
		ca_printf("CAS: Unable to start server beacon\n");
		return S_cas_noMemory;
	}
	
	return S_cas_success;
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

