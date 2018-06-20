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
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "osiSock.h"

#define epicsExportSharedSymbols
#include "noopiiu.h"

noopiiu noopIIU;

noopiiu::~noopiiu () 
{
}

unsigned noopiiu::getHostName ( 
    epicsGuard < epicsMutex > & cacGuard,
    char * pBuf, unsigned bufLength ) const throw ()
{
    return netiiu::getHostName ( cacGuard, pBuf, bufLength );
}

const char * noopiiu::pHostName (
    epicsGuard < epicsMutex > & cacGuard ) const throw ()
{
    return netiiu::pHostName ( cacGuard );
}

bool noopiiu::ca_v42_ok (
    epicsGuard < epicsMutex > & cacGuard ) const
{
    return netiiu::ca_v42_ok ( cacGuard );
}

bool noopiiu::ca_v41_ok (
    epicsGuard < epicsMutex > & cacGuard ) const
{
    return netiiu::ca_v41_ok ( cacGuard );
}

void noopiiu::writeRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, unsigned type, 
    arrayElementCount nElem, const void * pValue )
{
    netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void noopiiu::writeNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netWriteNotifyIO & io, unsigned type, 
    arrayElementCount nElem, const void *pValue )
{
    netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void noopiiu::readNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netReadNotifyIO & io, unsigned type, arrayElementCount nElem )
{
    netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void noopiiu::clearChannelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    ca_uint32_t sid, ca_uint32_t cid )
{
    netiiu::clearChannelRequest ( guard, sid, cid );
}

void noopiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netSubscription & subscr )
{
    netiiu::subscriptionRequest ( guard, chan, subscr );
}

void noopiiu::subscriptionUpdateRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netSubscription & subscr )
{
    netiiu::subscriptionUpdateRequest (
        guard, chan, subscr );
}

void noopiiu::subscriptionCancelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, netSubscription & subscr )
{
    netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void noopiiu::flushRequest ( 
    epicsGuard < epicsMutex > & guard )
{
    netiiu::flushRequest ( guard );
}

unsigned noopiiu::requestMessageBytesPending ( 
    epicsGuard < epicsMutex > & guard )
{
    return netiiu::requestMessageBytesPending ( guard );
}

void noopiiu::flush ( 
    epicsGuard < epicsMutex > & guard )
{
    netiiu::flush ( guard );
}

void noopiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & guard )
{
    netiiu::requestRecvProcessPostponedFlush ( guard );
}

osiSockAddr noopiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & guard ) const
{
    return netiiu::getNetworkAddress ( guard );
}

double noopiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & guard ) const
{
    return netiiu::receiveWatchdogDelay ( guard );
}

void noopiiu::uninstallChan ( 
    epicsGuard < epicsMutex > &, nciu & )
{
    // intentionally does not call default in netiiu
}

void noopiiu::uninstallChanDueToSuccessfulSearchResponse ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    const class epicsTime & currentTime )
{
    netiiu::uninstallChanDueToSuccessfulSearchResponse (
        guard, chan, currentTime );
}

bool noopiiu::searchMsg (
    epicsGuard < epicsMutex > & guard, ca_uint32_t id, 
        const char * pName, unsigned nameLength )
{
    return netiiu::searchMsg (
        guard, id, pName, nameLength );
}

