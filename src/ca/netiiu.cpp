
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

#include <limits.h>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "cac.h"
#include "netiiu.h"

template class tsSLNode < nciu >;

netiiu::netiiu ( cac *pClientCtxIn ) : pClientCtx ( pClientCtxIn )
{
}

netiiu::~netiiu ()
{
}

void netiiu::show ( unsigned level ) const
{
    ::printf ( "network IO base class\n" );
    if ( level > 1 ) {
        tsDLIterConstBD < nciu > pChan = this->channelList.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 1u );
            pChan++;
        }
    }
    if ( level > 2u ) {
        ::printf ( "\tcac pointer %p\n", 
            static_cast <void *> ( this->pClientCtx ) );
    }
}

void netiiu::connectTimeoutNotify ()
{
    tsDLIterBD < nciu > chan = this->channelList.firstIter ();
    while ( chan.valid () ) {
        chan->connectTimeoutNotify ();
        chan++;
    }
}

void netiiu::resetChannelRetryCounts ()
{
    tsDLIterBD < nciu > chan = this->channelList.firstIter ();
    while ( chan.valid () ) {
        chan->resetRetryCount ();
        chan++;
    }
}

bool netiiu::searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel )
{
    bool success;

    if ( nciu *pChan = this->channelList.get () ) {
        success = pChan->searchMsg ( retrySeqNumber, retryNoForThisChannel );
        if ( success ) {
            this->channelList.add ( *pChan );
        }
        else {
            this->channelList.push ( *pChan );
        }
    }
    else {
        success = false;
    }

    return success;
}

bool netiiu::ca_v42_ok () const
{
    return false;
}

bool netiiu::pushDatagramMsg ( const caHdr &, const void *, ca_uint16_t )
{
    return false;
}

bool netiiu::isVirtaulCircuit ( const char *, const osiSockAddr & ) const
{
    return false;
}

void netiiu::lastChannelDetachNotify ()
{
}

void netiiu::writeRequest ( nciu &, unsigned, unsigned, const void * )
{
    throw cacChannel::notConnected();
}

void netiiu::writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned, unsigned, const void * )
{
    throw cacChannel::notConnected();
}

void netiiu::readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned, unsigned )
{
    throw cacChannel::notConnected();
}

void netiiu::createChannelRequest ( nciu & )
{
}

void netiiu::clearChannelRequest ( ca_uint32_t sid, ca_uint32_t cid )
{
}

void netiiu::subscriptionRequest ( nciu &, netSubscription & )
{
}

void netiiu::subscriptionCancelRequest (  nciu & chan, netSubscription & subscr )
{
}

void netiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->pHostName (), bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

const char * netiiu::pHostName () const
{
    return "<disconnected>";
}

osiSockAddr netiiu::getNetworkAddress () const
{
    osiSockAddr addr;
    addr.sa.sa_family = AF_UNSPEC;
    return addr;
}

void netiiu::flushRequest ()
{
}

bool netiiu::flushBlockThreshold () const
{
    return false;
}

void netiiu::flushRequestIfAboveEarlyThreshold ()
{
}

void netiiu::blockUntilSendBacklogIsReasonable ( epicsMutex *, epicsMutex & )
{
}

int netiiu::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->pClientCtx->vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void netiiu::requestRecvProcessPostponedFlush ()
{
    return;
}

