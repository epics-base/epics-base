
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

#include "iocinf.h"
#include "netiiu_IL.h"
#include "nciu_IL.h"

#include "claimMsgCache_IL.h"

netiiu::~netiiu ()
{
    assert ( this->channelList.count () == 0u );
}

void netiiu::show ( unsigned level ) const
{
    this->lock ();
    printf ( "network IO base class\n" );
    if ( level > 1 ) {
        tsDLIterConstBD < nciu > pChan ( this->channelList.first () );
	    while ( pChan.valid () ) {
            pChan->show ( level - 1u );
            pChan = pChan.itemAfter ();
        }
    }
    if ( level > 2u ) {
        printf ("\tcac pointer %p\n", &this->cacRef );
        this->mutex.show ( level - 2u );
    }
    this->unlock ();
}

unsigned netiiu::channelCount () const
{
    return this->channelList.count ();
}

void netiiu::lock () const
{
    this->mutex.lock ();
}

void netiiu::unlock () const
{
    this->mutex.unlock ();
}

void netiiu::detachAllChan ()
{
    this->lock ();
    tsDLIterBD <nciu> chan ( this->channelList.first () );
    while ( chan.valid () ) {
        tsDLIterBD <nciu> next = chan.itemAfter ();
        chan->detachChanFromIIU ();
        chan = next;
    }
    this->unlock ();
}

void netiiu::disconnectAllChan ()
{
    this->lock ();
    tsDLIterBD <nciu> chan ( this->channelList.first () );
    while ( chan.valid () ) {
        tsDLIterBD <nciu> next = chan.itemAfter ();
        chan->disconnect ();
        chan = next;
    }
    this->unlock ();
}

void netiiu::connectTimeoutNotify ()
{
    this->lock ();
    tsDLIterBD <nciu> chan ( this->channelList.first () );
    while ( chan.valid () ) {
        chan->connectTimeoutNotify ();
        chan++;
    }
    this->unlock ();
}

void netiiu::resetChannelRetryCounts ()
{
    this->lock ();
    tsDLIterBD <nciu> chan ( this->channelList.first () );
    while ( chan.valid () ) {
        chan->resetRetryCount ();
        chan++;
    }
    this->unlock ();
}

bool netiiu::searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel )
{
    bool status;

    this->lock ();

    tsDLIterBD <nciu> chan = this->channelList.first ();
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

    this->unlock ();

    return status;
}

//
// considerable extra effort is taken in this routine to 
// guarantee that the lock is not held while blocking 
// in ::send () for buffer space.
//
void netiiu::sendPendingClaims ( bool v42Ok, claimMsgCache &cache )
{
    while ( 1 ) {
        this->lock ();
        tsDLIterBD < nciu > chan ( this->channelList.first () );
        if ( ! chan.valid () ) {
            this->unlock ();
            return;
        }
        if ( chan->claimSent () ) {
            this->unlock ();
            return;
        }
        if ( ! chan->setClaimMsgCache ( cache ) ) {
            this->unlock ();
            return;
        }
        // move channel to the end of the list so that it
        // will not be considered again for a claim message
        this->channelList.remove ( *chan );
        this->channelList.add ( *chan );
        this->unlock ();
        
        int status = cache.deliverMsg ( *this );
        if ( status != ECA_NORMAL ) {
            // this indicates diconnect condition 
            // therefore no cleanup required
            return;
        }

        if ( ! v42Ok ) {
            cache.connectChannel ( this->cacRef );
        }
    }
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

bool netiiu::connectionInProgress ( const char *, const osiSockAddr & ) const
{
    return false;
}

void netiiu::lastChannelDetachNotify ()
{
}

int netiiu::writeRequest ( unsigned, unsigned, unsigned, const void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::writeNotifyRequest ( unsigned, unsigned, unsigned, unsigned, const void * )
{
    return ECA_DISCONNCHID;
}

int netiiu::readCopyRequest ( unsigned, unsigned, unsigned, unsigned )
{
    return ECA_DISCONNCHID;
}

int netiiu::readNotifyRequest ( unsigned, unsigned, unsigned, unsigned )
{
    return ECA_DISCONNCHID;
}

int netiiu::createChannelRequest ( unsigned, const char *, unsigned )
{
    return ECA_DISCONNCHID;
}

int netiiu::clearChannelRequest ( unsigned, unsigned )
{
    return ECA_DISCONNCHID;
}

int netiiu::subscriptionRequest ( unsigned, unsigned, unsigned, unsigned, unsigned, bool )
{
    return ECA_DISCONNCHID;
}

int netiiu::subscriptionCancelRequest ( unsigned, unsigned, unsigned, unsigned )
{
    return ECA_DISCONNCHID;
}


