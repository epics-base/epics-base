
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "fdManager.h"
#include "envDefs.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "caServerI.h"
#include "beaconTimer.h"

// the maximum beacon period if EPICS_CA_BEACON_PERIOD isnt available
static const double CAServerMaxBeaconPeriod = 15.0; // seconds

// the initial beacon period
static const double CAServerMinBeaconPeriod = 1.0e-3; // seconds

beaconTimer::beaconTimer ( caServerI & casIn ) :
    timer ( fileDescriptorManager.createTimer() ), 
    cas ( casIn ),
    beaconPeriod ( CAServerMinBeaconPeriod ), 
    maxBeaconInterval ( CAServerMaxBeaconPeriod ),
    beaconCounter ( 0U )
{
    caStatus status;
	double maxPeriod;

    if ( envGetConfigParamPtr ( & EPICS_CAS_BEACON_PERIOD ) ) {
        status = envGetDoubleConfigParam ( & EPICS_CAS_BEACON_PERIOD, & maxPeriod );
    }
    else {
        status = envGetDoubleConfigParam ( & EPICS_CA_BEACON_PERIOD, & maxPeriod );
    }
	if ( status || maxPeriod <= 0.0 ) {
		errlogPrintf (
			"EPICS \"%s\" float fetch failed\n", EPICS_CAS_BEACON_PERIOD.name );
		errlogPrintf (
			"Setting \"%s\" = %f\n", EPICS_CAS_BEACON_PERIOD.name, 
			this->maxBeaconInterval);
	}
	else {
		this->maxBeaconInterval = maxPeriod;
	}

    this->timer.start ( *this, CAServerMinBeaconPeriod );
}

beaconTimer::~beaconTimer ()
{
    this->timer.destroy ();
}

void beaconTimer::generateBeaconAnomaly ()
{
    this->beaconPeriod = CAServerMinBeaconPeriod;
    this->timer.start ( *this, CAServerMinBeaconPeriod );
}


epicsTimerNotify::expireStatus beaconTimer::expire( const epicsTime & /* currentTime */ )	
{
	this->cas.sendBeacon ( this->beaconCounter );

    this->beaconCounter++;
 
	// double the period between beacons (but dont exceed max)
	if ( this->beaconPeriod < this->maxBeaconInterval ) {
	    this->beaconPeriod += this->beaconPeriod;

	    if ( this->beaconPeriod >= this->maxBeaconInterval ) {
		    this->beaconPeriod = this->maxBeaconInterval;
	    }
    }

    return expireStatus ( restart, this->beaconPeriod );
}

