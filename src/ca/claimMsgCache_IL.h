
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

inline claimMsgCache::claimMsgCache ( bool v44In ) :
    pStr ( 0 ), clientId ( UINT_MAX ), serverId ( UINT_MAX ), currentStrLen ( 0u ), 
        bufLen ( 0u ), v44 ( v44In )
{
}

inline claimMsgCache::~claimMsgCache ()
{
    if ( this->pStr ) {
        delete this->pStr;
    }
}

inline int claimMsgCache::deliverMsg ( netiiu &iiu )
{
    if ( v44 ) {
        return iiu.createChannelRequest ( this->clientId, this->pStr, this->currentStrLen );
    }
    else {
        return iiu.createChannelRequest ( this->serverId, 0u, 0u );
    }
}

inline void claimMsgCache::connectChannel ( cac & cacRef )
{
    cacRef.connectChannel ( this->clientId );
}
