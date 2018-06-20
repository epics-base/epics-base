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
#include "nciu.h"

static const double disconnectGovernorPeriod = 10.0; // sec

disconnectGovernorTimer::disconnectGovernorTimer ( 
    disconnectGovernorNotify & iiuIn, 
    epicsTimerQueue & queueIn, 
    epicsMutex & mutexIn ) :
        mutex ( mutexIn ), timer ( queueIn.createTimer () ),
    iiu ( iiuIn )
{
}

disconnectGovernorTimer::~disconnectGovernorTimer ()
{
    this->timer.destroy ();
}

void disconnectGovernorTimer:: start ()
{
    this->timer.start ( *this, disconnectGovernorPeriod );
}

void disconnectGovernorTimer::shutdown (
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        {
            epicsGuardRelease < epicsMutex > cbUnguard ( cbGuard );
            this->timer.cancel ();
        }
    }
    while ( nciu * pChan = this->chanList.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }
}

epicsTimerNotify::expireStatus disconnectGovernorTimer::expire ( 
    const epicsTime & /* currentTime */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    while ( nciu * pChan = chanList.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        this->iiu.govExpireNotify ( guard, *pChan );
    }
    return expireStatus ( restart, disconnectGovernorPeriod );
}

void disconnectGovernorTimer::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    ::printf ( "disconnect governor timer: with %u channels pending\n",
        this->chanList.count () );
    if ( level > 0u ) {
        tsDLIterConst < nciu > pChan = this->chanList.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 1u );
            pChan++;
        }
    }
}

void disconnectGovernorTimer::installChan ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->chanList.add ( chan );
    chan.channelNode::listMember = channelNode::cs_disconnGov;
}

void disconnectGovernorTimer::uninstallChan (
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->chanList.remove ( chan );
    chan.channelNode::listMember = channelNode::cs_none;
}

disconnectGovernorNotify::~disconnectGovernorNotify () {}

