
/*
 *
 * caServerOS.c
 * $Id$
 *
 *
 * $Log$
 *
 */


//
// CA server
// 
#include <server.h>

#define nSecPerUSec 1000

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
osiBool casBeaconTimer::again()	
{
	return osiTrue; 
}

//
// casBeaconTimer::delay()
//
const osiTime casBeaconTimer::delay()	
{
	return os->getBeaconPeriod();
}


//
// caServerOS::init()
//
caStatus caServerOS::init()
{
	(*this)->setNonBlocking();

	this->pRdReg = new casServerReg(*this);
	if (!this->pRdReg) {
		return S_cas_noMemory;
	}

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
	if (this->pRdReg) {
		delete this->pRdReg;
	}
	if (this->pBTmr) {
		delete this->pBTmr;
	}
}


//
// casServerReg::callBack()
//
void casServerReg::callBack()
{
	assert(os.pRdReg);
	os->connectCB();	
}

//
// casServerReg::~casServerReg()
//
casServerReg::~casServerReg()
{
	this->os.pRdReg = NULL;
}

//
// caServerOS::getFD()
//
int caServerOS::getFD()
{
	 return cas.caServerIO::getFD();
}

