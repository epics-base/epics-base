/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <stdexcept>

#include <limits.h>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "cac.h"
#include "netiiu.h"

netiiu::~netiiu ()
{
}

bool netiiu::ca_v42_ok (
    epicsGuard < epicsMutex > & ) const
{
    return false;
}

bool netiiu::ca_v41_ok (
    epicsGuard < epicsMutex > & ) const
{
    return false;
}

void netiiu::writeRequest ( 
    epicsGuard < epicsMutex > &, nciu &, 
    unsigned, arrayElementCount, const void * )
{
    throw cacChannel::notConnected();
}

void netiiu::writeNotifyRequest ( 
    epicsGuard < epicsMutex > &, 
    nciu &, netWriteNotifyIO &, unsigned, 
    arrayElementCount, const void * )
{
    throw cacChannel::notConnected();
}

void netiiu::readNotifyRequest ( 
    epicsGuard < epicsMutex > &, 
    nciu &, netReadNotifyIO &, unsigned, arrayElementCount )
{
    throw cacChannel::notConnected();
}

void netiiu::clearChannelRequest ( 
    epicsGuard < epicsMutex > &, ca_uint32_t, ca_uint32_t )
{
}

void netiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > &, nciu &, netSubscription & )
{
}

void netiiu::subscriptionCancelRequest ( 
    epicsGuard < epicsMutex > &, nciu &, netSubscription & )
{
}

void netiiu::subscriptionUpdateRequest ( 
    epicsGuard < epicsMutex > &, nciu &, netSubscription & )
{
}

void netiiu::hostName ( 
    epicsGuard < epicsMutex > & guard,
    char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->pHostName ( guard ), bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

const char * netiiu::pHostName (
    epicsGuard < epicsMutex > & ) const
{
    return "<disconnected>";
}

osiSockAddr netiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & ) const
{
    osiSockAddr addr;
    addr.sa.sa_family = AF_UNSPEC;
    return addr;
}

void netiiu::flushRequest ( 
    epicsGuard < epicsMutex > & )
{
}

void netiiu::eliminateExcessiveSendBacklog ( 
    epicsGuard < epicsMutex > *,
    epicsGuard < epicsMutex > & )
{
}

void netiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & )
{
}

void netiiu::uninstallChan ( 
    epicsGuard < epicsMutex > &, nciu & )
{
    throw cacChannel::notConnected();
}

double netiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & guard ) const
{
    return - DBL_MAX;
}

void netiiu::uninstallChanDueToSuccessfulSearchResponse ( 
    epicsGuard < epicsMutex > &, nciu &, 
    const epicsTime & currentTime )
{
    throw std::runtime_error ( 
        "search response occured when not attached to udpiiu?" );
}

bool netiiu::searchMsg (
    epicsGuard < epicsMutex > &, ca_uint32_t id, 
        const char * pName, unsigned nameLength )
{
    return false;
}


