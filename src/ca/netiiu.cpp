
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
#include "nciu_IL.h"
#include "claimMsgCache_IL.h"

netiiu::~netiiu ()
{
}

void netiiu::show ( unsigned /* level */ ) const
{
    this->lock ();

    tsDLIterConstBD <nciu> pChan ( this->chidList.first () );
	while ( pChan.valid () ) {
        char hostName [256];
		printf(	"%s native type=%d ", 
			pChan->pName (), pChan->nativeType () );
        pChan->hostName ( hostName, sizeof (hostName) );
		printf(	"N elements=%lu server=%s state=", 
			pChan->nativeElementCount (), hostName );
		switch ( pChan->state () ) {
		case cs_never_conn:
			printf ( "never connected to an IOC" );
			break;
		case cs_prev_conn:
			printf ( "disconnected from IOC" );
			break;
		case cs_conn:
			printf ( "connected to an IOC" );
			break;
		case cs_closed:
			printf ( "invalid channel" );
			break;
		default:
			break;
		}
		printf ( "\n" );
	}

    this->unlock ();

}

unsigned netiiu::channelCount () const
{
    return this->chidList.count ();
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
    tsDLIterBD <nciu> chan ( this->chidList.first () );
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
    tsDLIterBD <nciu> chan ( this->chidList.first () );
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
    tsDLIterBD <nciu> chan ( this->chidList.first () );
    while ( chan.valid () ) {
        chan->connectTimeoutNotify ();
        chan++;
    }
    this->unlock ();
}

void netiiu::resetChannelRetryCounts ()
{
    this->lock ();
    tsDLIterBD <nciu> chan ( this->chidList.first () );
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

    tsDLIterBD <nciu> chan = this->chidList.first ();
    if ( chan.valid () ) {
        status = chan->searchMsg ( retrySeqNumber, retryNoForThisChannel );
        if ( status ) {
            this->chidList.remove ( *chan );
            this->chidList.add ( *chan );
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
void netiiu::sendPendingClaims ( tcpiiu &iiu, bool v42Ok, claimMsgCache &cache )
{
    while ( 1 ) {
        while (1) {
            this->lock ();
            tsDLIterBD < nciu > chan ( this->chidList.last () );
            if ( ! chan.valid () ) {
                this->unlock ();
                return;
            }

            bool status = cache.set ( *chan );
            if ( status ) {
                this->unlock ();
                break;
            }

            this->unlock ();
            threadSleep ( 1.0 );
        }
        
        int status = cache.deliverMsg ( iiu );
        if ( status != ECA_NORMAL ) {
            break;
        }

        this->lock ();
        // if the channel was not deleted while the lock was off
        tsDLIterBD < nciu > chan ( this->chidList.last () );
        if ( chan.valid () ) {
            if ( cache.channelMatches ( *chan ) ) {
                if ( ! v42Ok ) {
                    chan->connect ( iiu );
                }
                chan->attachChanToIIU ( iiu );
            }
        }
        this->unlock ();
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

bool netiiu::connectionInProgress ( const char *pChannelName, const osiSockAddr &addr ) const
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

int netiiu::writeNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, const void *pValue )
{
    return ECA_DISCONNCHID;
}

int netiiu::readCopyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    return ECA_DISCONNCHID;
}

int netiiu::readNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    return ECA_DISCONNCHID;
}

int netiiu::createChannelRequest ( unsigned clientId, const char *pName, unsigned nameLength )
{
    return ECA_DISCONNCHID;
}

int netiiu::clearChannelRequest ( unsigned clientId, unsigned serverId )
{
    return ECA_DISCONNCHID;
}

int netiiu::subscriptionRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, unsigned mask )
{
    return ECA_DISCONNCHID;
}

int netiiu::subscriptionCancelRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    return ECA_DISCONNCHID;
}