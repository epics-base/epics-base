
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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef beaconAnomalyGovernorh
#define beaconAnomalyGovernorh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_beaconAnomalyGovernorh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "epicsTimer.h"
#include "epicsMutex.h"

#ifdef epicsExportSharedSymbols_beaconAnomalyGovernorh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caServerI;

class beaconAnomalyGovernor : public epicsTimerNotify {
public:
	beaconAnomalyGovernor ( caServerI & );
	virtual ~beaconAnomalyGovernor();
    void start ();
	void show ( unsigned level ) const;
private:
    // has been checked for thread safety
    epicsTimer & timer;
	class caServerI & cas;
    bool anomalyPending;
	expireStatus expire( const epicsTime & currentTime );
	beaconAnomalyGovernor ( const beaconAnomalyGovernor & );
	beaconAnomalyGovernor & operator = ( const beaconAnomalyGovernor & );
};

#endif // ifdef beaconAnomalyGovernorh

