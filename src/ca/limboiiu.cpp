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
 *  $Id$
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "netiiu.h"
#include "inetAddrID.h"

limboiiu::limboiiu ()
{
}

void limboiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    netiiu::hostName ( pBuf, bufLength );
}

const char * limboiiu::pHostName () const
{
    return netiiu::pHostName ();
}

bool limboiiu::ca_v42_ok () const
{
    return netiiu::ca_v42_ok ();
}

void limboiiu::writeRequest ( epicsGuard < cacMutex > & guard, nciu & chan, unsigned type, 
                unsigned nElem, const void * pValue )
{
    netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void limboiiu::writeNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netWriteNotifyIO & io, unsigned type, unsigned nElem, const void *pValue )
{
    netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void limboiiu::readNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netReadNotifyIO & io, unsigned type, unsigned nElem )
{
    netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void limboiiu::clearChannelRequest ( epicsGuard < cacMutex > & guard, 
                ca_uint32_t sid, ca_uint32_t cid )
{
    netiiu::clearChannelRequest ( guard, sid, cid );
}

void limboiiu::subscriptionRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netSubscription & subscr )
{
    netiiu::subscriptionRequest ( guard, chan, subscr );
}

void limboiiu::subscriptionCancelRequest ( epicsGuard < cacMutex > & guard, 
                nciu & chan, netSubscription & subscr )
{
    netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void limboiiu::flushRequest ()
{
    netiiu::flushRequest ();
}

bool limboiiu::flushBlockThreshold ( epicsGuard < cacMutex > & guard ) const
{
    return netiiu::flushBlockThreshold ( guard );
}

void limboiiu::flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & guard )
{
    netiiu::flushRequestIfAboveEarlyThreshold ( guard );
}

void limboiiu::blockUntilSendBacklogIsReasonable 
    ( cacNotify & notify, epicsGuard < cacMutex > & guard )
{
    netiiu::blockUntilSendBacklogIsReasonable ( notify, guard );
}

void limboiiu::requestRecvProcessPostponedFlush ()
{
    netiiu::requestRecvProcessPostponedFlush ();
}

osiSockAddr limboiiu::getNetworkAddress () const
{
    return netiiu::getNetworkAddress ();
}

void limboiiu::uninstallChan ( epicsGuard < cacMutex > & guard, nciu & chan )
{
    return netiiu::uninstallChan( guard, chan );
}






