

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

inline bool tcpiiu::fullyConstructed () const
{
    return this->fullyConstructedFlag;
}

inline void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    if ( this->pHostNameCache ) {
        this->pHostNameCache->hostName ( pBuf, bufLength );
    }
    else {
        netiiu::hostName ( pBuf, bufLength );
    }
}

// deprecated - please dont use - this is _not_ thread safe
inline const char * tcpiiu::pHostName () const
{
    static char nameBuf [128];
    this->hostName ( nameBuf, sizeof ( nameBuf ) );
    return nameBuf; // ouch !!
}

inline void tcpiiu::flushRequest ()
{
    epicsEventSignal ( this->sendThreadFlushSignal );
}

inline bool tcpiiu::ca_v44_ok () const
{
    return CA_V44 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
}

inline bool tcpiiu::alive () const
{
    if ( this->state == iiu_connecting || 
        this->state == iiu_connected ) {
        return true;
    }
    else {
        return false;
    }
}

inline bhe * tcpiiu::getBHE () const
{
    return this->pBHE;
}

inline void tcpiiu::beaconAnomalyNotify ()
{
    this->recvDog.beaconAnomalyNotify ();
}

inline void tcpiiu::beaconArrivalNotify ()
{
    this->recvDog.beaconArrivalNotify ();
}

inline void tcpiiu::fdCreateNotify ( epicsMutex &mutex, CAFDHANDLER *func, void *pArg )
{
    if ( this->fdRegCallbackNeeded ) {
        epicsAutoMutexRelease autoRelease ( mutex );
        ( *func ) ( pArg, this->sock, true );
    }
}

inline void tcpiiu::fdDestroyNotify ( CAFDHANDLER *func, void *pArg )
{
    ( *func ) ( pArg, this->sock, false );
}
