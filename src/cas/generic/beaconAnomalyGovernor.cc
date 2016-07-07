
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

#include "fdManager.h"

#define epicsExportSharedSymbols
#include "caServerI.h"
#include "beaconAnomalyGovernor.h"
#include "beaconTimer.h"

//
// the minimum period between beacon anomalies
// -------------------------------------------
//
// o Gateways imply a tradeoff towards less traffic while
// paying a price of a less tightly coupled environment.
// Therefore, clients should not expect instant reconnects 
// when a server is rebooted on the other side of a gateway. 
//
// o In a quiescent system servers should not need to be 
// rebooted and network connectivity should be constant.
//
// o Clients will keep trying to reach channels for around
// 15 min after the last beacon anomaly was seen.
//
// o The exponential backoff mechanism in the beacon timer
// ensures that clients will detect beacon anomalies 
// continuously for around 30 seconds after each beacon 
// anomaly is requested.
//
static const double CAServerMinBeaconAnomalyPeriod = 60.0 * 5.0; // seconds

beaconAnomalyGovernor::beaconAnomalyGovernor ( caServerI & casIn ) :
    timer ( fileDescriptorManager.createTimer () ), cas ( casIn ),
    anomalyPending ( false )
{
}

beaconAnomalyGovernor::~beaconAnomalyGovernor()
{
}

void beaconAnomalyGovernor::start ()
{
    // order of operations here impacts thread safety
    this->anomalyPending = true;
    epicsTimer::expireInfo expInfo = this->timer.getExpireInfo ();
    if ( ! expInfo.active ) {
        this->cas.beaconTmr.generateBeaconAnomaly ();
        this->timer.start ( *this, CAServerMinBeaconAnomalyPeriod );
        this->anomalyPending = false;
    }
}

epicsTimerNotify::expireStatus beaconAnomalyGovernor::expire ( const epicsTime & )
{
    if ( this->anomalyPending ) {
        this->anomalyPending = false;
        this->cas.beaconTmr.generateBeaconAnomaly ();
    }
    return noRestart;
}

void beaconAnomalyGovernor::show ( unsigned level ) const
{
    printf ( "beaconAnomalyGovernor: anomalyPending = %s\n",
        this->anomalyPending ? "T": "F" );
    if ( level ) {
        this->timer.show ( level - 1 );
    }
}
