/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

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
    expireStatus expire ( const epicsTime & currentTime );
	casBeaconTimer ( const casBeaconTimer & );
	casBeaconTimer & operator = ( const casBeaconTimer & );
};

casBeaconTimer::casBeaconTimer ( double delay, caServerOS &osIn ) :
    timer ( fileDescriptorManager.createTimer() ), os ( osIn ) 
{
    this->timer.start ( *this, delay );
}

casBeaconTimer::~casBeaconTimer ()
{
    this->timer.destroy ();
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
epicsTimerNotify::expireStatus casBeaconTimer::expire( const epicsTime & /* currentTime */ )	
{
	os.sendBeacon ();
    return expireStatus ( restart, os.getBeaconPeriod() );
}

