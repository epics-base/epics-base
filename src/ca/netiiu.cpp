
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

#include "iocinf.h"
#include "netiiu_IL.h"
#include "nciu_IL.h"

netiiu::netiiu ( cac *pClientCtxIn ) : pClientCtx ( pClientCtxIn )
{
}

netiiu::~netiiu ()
{
    assert ( this->channelList.count () == 0u );
}

void netiiu::show ( unsigned level ) const
{
    osiAutoMutex autoMutex ( this->mutex );

    printf ( "network IO base class\n" );
    if ( level > 1 ) {
        tsDLIterConstBD < nciu > pChan ( this->channelList.first () );
	    while ( pChan.valid () ) {
            pChan->show ( level - 1u );
            pChan = pChan.itemAfter ();
        }
    }
    if ( level > 2u ) {
        printf ( "\tcac pointer %p\n", this->pClientCtx );
        this->mutex.show ( level - 2u );
    }
}

unsigned netiiu::channelCount () const
{
    return this->channelList.count ();
}

// cac lock must also be applied when
// calling this
void netiiu::attachChannel ( nciu &chan )
{
    osiAutoMutex autoMutex ( this->mutex );
    this->channelList.add ( chan );
}

// cac lock must also be applied when
// calling this
void netiiu::detachChannel ( nciu &chan )
{
    {
        osiAutoMutex autoMutex ( this->mutex );
        this->channelList.remove ( chan );
        if ( this->channelList.count () == 0u ) {
            this->lastChannelDetachNotify ();
        }
    }
}

// cac lock must also be applied when
// calling this
void netiiu::disconnectAllChan ( netiiu & newiiu )
{
    tsDLList < nciu > list;

    {
        osiAutoMutex autoMutex ( this->mutex );
        tsDLIterBD < nciu > chan ( this->channelList.first () );
        while ( chan.valid () ) {
            tsDLIterBD < nciu > next = chan.itemAfter ();
            this->clearChannelRequest ( *chan );
            this->channelList.remove ( *chan );
            chan->disconnect ( newiiu );
            list.add ( *chan );
            chan = next;
        }
    }

    {
        osiAutoMutex autoMutex ( newiiu.mutex );
        newiiu.channelList.add ( list );
    }
}

void netiiu::connectTimeoutNotify ()
{
    osiAutoMutex autoMutex ( this->mutex );
    tsDLIterBD < nciu > chan ( this->channelList.first () );
    while ( chan.valid () ) {
        chan->connectTimeoutNotify ();
        chan++;
    }
}

void netiiu::resetChannelRetryCounts ()
{
    osiAutoMutex autoMutex ( this->mutex );
    tsDLIterBD < nciu > chan ( this->channelList.first () );
    while ( chan.valid () ) {
        chan->resetRetryCount ();
        chan++;
    }
}

bool netiiu::searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel )
{
    bool status;

    osiAutoMutex autoMutex ( this->mutex );

    tsDLIterBD < nciu > chan = this->channelList.first ();
    if ( chan.valid () ) {
        status = chan->searchMsg ( retrySeqNumber, retryNoForThisChannel );
        if ( status ) {
            this->channelList.remove ( *chan );
            this->channelList.add ( *chan );
        }
    }
    else {
        status = false;
    }


    return status;
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

int netiiu::writeNotifyRequest ( nciu &, cacNotify &, unsigned, unsigned, const void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::readCopyRequest ( nciu &, unsigned, unsigned, void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::readNotifyRequest ( nciu &, cacNotify &, unsigned, unsigned )
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

int netiiu::subscriptionRequest ( netSubscription &, bool )
{
    return ECA_NORMAL;
}

int netiiu::subscriptionCancelRequest ( netSubscription & )
{
    return ECA_DISCONNCHID;
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

void netiiu::subscribeAllIO ( nciu & )
{
}