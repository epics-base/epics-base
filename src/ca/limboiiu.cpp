
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
    return netiiu::hostName ( pBuf, bufLength );
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
    return netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void limboiiu::writeNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netWriteNotifyIO & io, unsigned type, unsigned nElem, const void *pValue )
{
    return netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void limboiiu::readNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netReadNotifyIO & io, unsigned type, unsigned nElem )
{
    return netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void limboiiu::clearChannelRequest ( epicsGuard < cacMutex > & guard, 
                ca_uint32_t sid, ca_uint32_t cid )
{
    return netiiu::clearChannelRequest ( guard, sid, cid );
}

void limboiiu::subscriptionRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netSubscription & subscr )
{
    return netiiu::subscriptionRequest ( guard, chan, subscr );
}

void limboiiu::subscriptionCancelRequest ( epicsGuard < cacMutex > & guard, 
                nciu & chan, netSubscription & subscr )
{
    return netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void limboiiu::flushRequest ()
{
    return netiiu::flushRequest ();
}

bool limboiiu::flushBlockThreshold ( epicsGuard < cacMutex > & guard ) const
{
    return netiiu::flushBlockThreshold ( guard );
}

void limboiiu::flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & guard )
{
    return netiiu::flushRequestIfAboveEarlyThreshold ( guard );
}

void limboiiu::blockUntilSendBacklogIsReasonable 
    ( epicsGuard < callbackMutex > * pCBGuard, epicsGuard < cacMutex > & guard )
{
    return netiiu::blockUntilSendBacklogIsReasonable ( pCBGuard, guard );
}

void limboiiu::requestRecvProcessPostponedFlush ()
{
    return netiiu::requestRecvProcessPostponedFlush ();
}

osiSockAddr limboiiu::getNetworkAddress () const
{
    return netiiu::getNetworkAddress ();
}

void limboiiu::uninstallChannel ( epicsGuard < callbackMutex > & cbGuard, 
                               epicsGuard < cacMutex > & guard, nciu & chan )
{
    return netiiu::uninstallChannel ( cbGuard, guard, chan );
}






