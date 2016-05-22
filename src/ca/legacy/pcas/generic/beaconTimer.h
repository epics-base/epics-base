
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

#ifndef beaconTimerh
#define beaconTimerh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_beaconTimerh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "epicsTimer.h"
#include "caProto.h"

#ifdef epicsExportSharedSymbols_beaconTimerh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caServerI;

class beaconTimer : public epicsTimerNotify {
public:
    beaconTimer ( caServerI & casIn );
    virtual ~beaconTimer ();
    void generateBeaconAnomaly ();
private:
    // has been checked for thread safety
    epicsTimer & timer;
    caServerI & cas;
	double beaconPeriod;
	double maxBeaconInterval;
    ca_uint32_t beaconCounter;
    expireStatus expire ( const epicsTime & currentTime );
	beaconTimer ( const beaconTimer & );
	beaconTimer & operator = ( const beaconTimer & );
};

#endif // ifdef beaconTimerh

