/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// 
// $Id$
//
//                    L O S  A L A M O S
//              Los Alamos National Laboratory
//               Los Alamos, New Mexico 87545
//
//  Copyright, 1986, The Regents of the University of California.
//
//  Author: Jeff Hill
//

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "disconnectGovernorTimer.h"
#include "udpiiu.h"

static const double period = 10.0; // sec

disconnectGovernorTimer::disconnectGovernorTimer ( 
    udpiiu & iiuIn, epicsTimerQueue & queueIn ) :
        timer ( queueIn.createTimer () ),
    iiu ( iiuIn )
{
    this->timer.start ( *this, period );
}

disconnectGovernorTimer::~disconnectGovernorTimer ()
{
    this->timer.destroy ();
}

void disconnectGovernorTimer::shutdown ()
{
    this->timer.cancel ();
}

epicsTimerNotify::expireStatus disconnectGovernorTimer::expire ( const epicsTime & currentTime ) // X aCC 361
{
    this->iiu.govExpireNotify ( currentTime );
    return expireStatus ( restart, period );
}

void disconnectGovernorTimer::show ( unsigned /* level */ ) const
{
}

