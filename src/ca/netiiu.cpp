
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

#include "iocinf.h"
#include "netiiu_IL.h"
#include "nciu_IL.h"
#include "baseNMIU_IL.h"

netiiu::netiiu ( cac *pClientCtxIn ) : pClientCtx ( pClientCtxIn )
{
}

netiiu::~netiiu ()
{
}

void netiiu::show ( unsigned level ) const
{
    printf ( "network IO base class\n" );
    if ( level > 1 ) {
        tsDLIterConstBD < nciu > pChan = this->channelList.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 1u );
            pChan++;
        }
    }
    if ( level > 2u ) {
        printf ( "\tcac pointer %p\n", 
            static_cast <void *> ( this->pClientCtx ) );
    }
}

// cac lock must also be applied when calling this
void netiiu::disconnectAllChan ( netiiu & newiiu )
{
    tsDLIterBD < nciu > chan = this->channelList.firstIter ();
    while ( chan.valid () ) {
        tsDLIterBD < nciu > next = chan;
        next++;
        this->clearChannelRequest ( *chan );
        this->channelList.remove ( *chan );
        chan->disconnect ( newiiu );
        newiiu.channelList.add ( *chan );
        chan = next;
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

bool netiiu::ca_v41_ok () const
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

int netiiu::writeRequest ( nciu &, unsigned, unsigned, const void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned, unsigned, const void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned, unsigned )
{
    return ECA_DISCONNCHID;
}

int netiiu::createChannelRequest ( nciu & )
{
    return ECA_DISCONNCHID;
}

int netiiu::clearChannelRequest ( nciu & )
{
    return ECA_DISCONNCHID;
}

void netiiu::subscriptionRequest ( netSubscription & )
{
}

void netiiu::subscriptionCancelRequest ( netSubscription & )
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

void netiiu::disconnectAllIO ( nciu & )
{
}

void netiiu::connectAllIO ( nciu & )
{
}

double netiiu::beaconPeriod () const
{
    return ( - DBL_MAX );
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

void netiiu::blockUntilSendBacklogIsReasonable ( epicsMutex & )
{
}
